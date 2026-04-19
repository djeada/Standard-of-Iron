// Visual parity test for the GPU-skinned humanoid (Stage 15.5+).
//
// The rigged path (submit_humanoid_full_rigged / submit_humanoid_reduced_
// rigged) bakes the body geometry once at the bind pose and runtime
// composes a skinning matrix `palette[i] = current[i] * inverse_bind[i]`
// so the GPU reproduces the same per-vertex world position the imperative
// `submit_part_graph` walker would produce.
//
// This test executes that contract on the CPU side: it bakes the
// humanoid LOD with the canonical bind palette, walks the same PartGraph
// at a non-bind pose, then runs CPU LBS for every baked vertex and
// asserts the result matches the transformed unit-mesh vertex within a
// tight tolerance. It is the invariant that the existing identity-pose
// tests do not exercise: at the bind pose every skinning matrix collapses
// to identity, so any error in the inverse-bind / world-bake math hides.

#include "render/creature/part_graph.h"
#include "render/creature/spec.h"
#include "render/draw_queue.h"
#include "render/gl/primitives.h"
#include "render/humanoid/humanoid_renderer_base.h"
#include "render/humanoid/humanoid_spec.h"
#include "render/humanoid/skeleton.h"
#include "render/rigged_mesh.h"
#include "render/rigged_mesh_bake.h"
#include "render/rigged_mesh_cache.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

namespace {

using Render::Creature::CreatureLOD;
using Render::Creature::part_graph_for;
using Render::Creature::PrimitiveInstance;
using Render::Creature::PrimitiveShape;
using Render::GL::HumanoidPose;

auto build_pose(std::uint32_t seed, float t, bool moving) -> HumanoidPose {
  Render::GL::VariationParams var{};
  var.height_scale = 1.0F;
  var.bulk_scale = 1.0F;
  var.stance_width = 1.0F;
  var.arm_swing_amp = 1.0F;
  var.walk_speed_mult = 1.0F;
  HumanoidPose pose{};
  Render::GL::HumanoidRendererBase::compute_locomotion_pose(seed, t, moving,
                                                            var, pose);
  return pose;
}

// CPU mirror of the character_skinned vertex shader's skinning math.
auto cpu_skin(const Render::GL::RiggedVertex &v,
              const std::vector<QMatrix4x4> &palette) -> QVector3D {
  QMatrix4x4 skin{};
  skin.fill(0.0F);
  float wsum = 0.0F;
  for (int i = 0; i < 4; ++i) {
    float const w = v.bone_weights[i];
    if (w == 0.0F) {
      continue;
    }
    auto const idx = static_cast<std::size_t>(v.bone_indices[i]);
    if (idx >= palette.size()) {
      continue;
    }
    QMatrix4x4 const &p = palette[idx];
    for (int r = 0; r < 4; ++r) {
      for (int c = 0; c < 4; ++c) {
        skin(r, c) += w * p(r, c);
      }
    }
    wsum += w;
  }
  if (wsum < 1e-3F) {
    skin.setToIdentity();
  }
  QVector3D const a_pos{v.position_bone_local[0], v.position_bone_local[1],
                        v.position_bone_local[2]};
  return skin.map(a_pos);
}

void RunParityCheck(CreatureLOD lod) {
  auto const &spec = Render::Humanoid::humanoid_creature_spec();
  auto bind_span = Render::Humanoid::humanoid_bind_palette();

  Render::GL::RiggedMeshCache cache;
  const auto *entry = cache.get_or_bake(spec, lod, bind_span, 0);
  ASSERT_NE(entry, nullptr);
  ASSERT_NE(entry->mesh, nullptr);

  HumanoidPose const current_pose = build_pose(0xC0FFEEU, 0.4F, true);

  std::array<QMatrix4x4, Render::Humanoid::kBoneCount> current{};
  Render::Humanoid::BonePalette tmp{};
  Render::Humanoid::evaluate_skeleton(current_pose, QVector3D(1.0F, 0.0F, 0.0F),
                                      tmp);
  for (std::size_t i = 0; i < Render::Humanoid::kBoneCount; ++i) {
    current[i] = tmp[i];
  }

  std::vector<QMatrix4x4> palette;
  palette.reserve(entry->inverse_bind.size());
  for (std::size_t i = 0; i < entry->inverse_bind.size(); ++i) {
    palette.push_back(current[i] * entry->inverse_bind[i]);
  }

  auto const &graph = part_graph_for(spec, lod);
  auto const &baked_verts = entry->mesh->get_vertices();

  std::size_t baked_cursor = 0;
  std::size_t single_bone_checked = 0;
  std::size_t two_bone_checked = 0;

  for (PrimitiveInstance const &prim : graph.primitives) {
    if (prim.shape == PrimitiveShape::None) {
      continue;
    }
    Render::GL::Mesh *unit = nullptr;
    switch (prim.shape) {
    case PrimitiveShape::Sphere:
    case PrimitiveShape::OrientedSphere:
      unit = Render::GL::get_unit_sphere();
      break;
    case PrimitiveShape::Cylinder:
    case PrimitiveShape::OrientedCylinder:
      unit = Render::GL::get_unit_cylinder();
      break;
    case PrimitiveShape::Capsule:
      unit = Render::GL::get_unit_capsule();
      break;
    case PrimitiveShape::Cone:
      unit = Render::GL::get_unit_cone();
      break;
    case PrimitiveShape::Box:
      unit = Render::GL::get_unit_cube();
      break;
    case PrimitiveShape::Mesh:
      unit = prim.custom_mesh;
      break;
    default:
      break;
    }
    if (unit == nullptr) {
      continue;
    }
    auto const &src_verts = unit->get_vertices();
    if (src_verts.empty()) {
      continue;
    }

    bool const is_two_bone = (prim.shape == PrimitiveShape::Cylinder ||
                              prim.shape == PrimitiveShape::Capsule ||
                              prim.shape == PrimitiveShape::OrientedCylinder);

    if (!is_two_bone) {
      QVector3D centroid{0.0F, 0.0F, 0.0F};
      for (std::size_t k = 0; k < src_verts.size(); ++k) {
        auto const &bv = baked_verts[baked_cursor + k];
        QVector3D p = cpu_skin(bv, palette);
        EXPECT_TRUE(std::isfinite(p.x()))
            << "prim " << prim.debug_name << " vert " << k;
        EXPECT_TRUE(std::isfinite(p.y()));
        EXPECT_TRUE(std::isfinite(p.z()));
        centroid += p;
      }
      centroid /= static_cast<float>(src_verts.size());
      QVector3D const expected_centre =
          current[prim.params.anchor_bone].column(3).toVector3D();
      float const slack = std::max(prim.params.radius, 0.2F) * 1.5F;
      EXPECT_NEAR(centroid.x(), expected_centre.x(), slack)
          << "prim " << prim.debug_name;
      EXPECT_NEAR(centroid.y(), expected_centre.y(), slack)
          << "prim " << prim.debug_name;
      EXPECT_NEAR(centroid.z(), expected_centre.z(), slack)
          << "prim " << prim.debug_name;
      ++single_bone_checked;
    } else {
      QVector3D anchor_centroid{0.0F, 0.0F, 0.0F};
      QVector3D tail_centroid{0.0F, 0.0F, 0.0F};
      std::size_t anchor_n = 0;
      std::size_t tail_n = 0;
      for (std::size_t k = 0; k < src_verts.size(); ++k) {
        auto const &bv = baked_verts[baked_cursor + k];
        QVector3D p = cpu_skin(bv, palette);
        EXPECT_TRUE(std::isfinite(p.x()));
        EXPECT_TRUE(std::isfinite(p.y()));
        EXPECT_TRUE(std::isfinite(p.z()));
        float const sy = src_verts[k].position[1];
        if (std::abs(sy + 0.5F) < 1e-4F) {
          anchor_centroid += p;
          ++anchor_n;
        } else if (std::abs(sy - 0.5F) < 1e-4F) {
          tail_centroid += p;
          ++tail_n;
        }
      }
      ASSERT_GT(anchor_n, 0U);
      ASSERT_GT(tail_n, 0U);
      anchor_centroid /= static_cast<float>(anchor_n);
      tail_centroid /= static_cast<float>(tail_n);

      auto bwo = [](const QMatrix4x4 &m, const QVector3D &local) {
        QVector3D const o = m.column(3).toVector3D();
        QVector3D const x = m.column(0).toVector3D();
        QVector3D const y = m.column(1).toVector3D();
        QVector3D const z = m.column(2).toVector3D();
        return o + x * local.x() + y * local.y() + z * local.z();
      };
      QVector3D const exp_anchor =
          bwo(current[prim.params.anchor_bone], prim.params.head_offset);
      QVector3D const exp_tail =
          bwo(current[prim.params.tail_bone], prim.params.tail_offset);

      float const slack = std::max(prim.params.radius, 0.05F) * 1.5F;
      EXPECT_NEAR(anchor_centroid.x(), exp_anchor.x(), slack)
          << "prim " << prim.debug_name << " anchor end";
      EXPECT_NEAR(anchor_centroid.y(), exp_anchor.y(), slack)
          << "prim " << prim.debug_name << " anchor end";
      EXPECT_NEAR(anchor_centroid.z(), exp_anchor.z(), slack)
          << "prim " << prim.debug_name << " anchor end";
      EXPECT_NEAR(tail_centroid.x(), exp_tail.x(), slack)
          << "prim " << prim.debug_name << " tail end";
      EXPECT_NEAR(tail_centroid.y(), exp_tail.y(), slack)
          << "prim " << prim.debug_name << " tail end";
      EXPECT_NEAR(tail_centroid.z(), exp_tail.z(), slack)
          << "prim " << prim.debug_name << " tail end";
      ++two_bone_checked;
    }

    baked_cursor += src_verts.size();
  }

  EXPECT_GT(single_bone_checked, 0U);
  EXPECT_GT(two_bone_checked, 0U);
  EXPECT_EQ(baked_cursor, baked_verts.size())
      << "test walked a different primitive set than the bake";
}

TEST(RiggedVisualParity, BindPaletteIsInvertibleAndOrthonormal) {
  auto bind = Render::Humanoid::humanoid_bind_palette();
  for (std::size_t i = 0; i < bind.size(); ++i) {
    bool ok = false;
    bind[i].inverted(&ok);
    EXPECT_TRUE(ok) << "bind[" << i << "] not invertible";
    EXPECT_NEAR(std::abs(bind[i].determinant()), 1.0F, 1e-3F)
        << "bind[" << i << "]";
  }
}

TEST(RiggedVisualParity, LbsMatchesImperativeWalkerForReducedLod) {
  RunParityCheck(CreatureLOD::Reduced);
}

TEST(RiggedVisualParity, LbsMatchesImperativeWalkerForFullLod) {
  RunParityCheck(CreatureLOD::Full);
}

} // namespace
