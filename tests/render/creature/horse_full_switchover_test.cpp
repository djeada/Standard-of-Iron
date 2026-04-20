// Stage 15.5e — horse Full + Reduced LOD switchover regression.
//
// Verifies that the static k_horse_full_parts (anatomically enriched
// to 43 prims) and k_horse_reduced_parts (11 prims) PartGraphs each
// bake into a single RiggedMesh, and that the bone palette resolved
// from a per-entity pose is well-formed. Also exercises the cache
// key: two units with different `variation_scale` values must hit
// the SAME mesh pointer (proving variation flows through the cmd,
// not the bake).

#include "render/bone_palette_arena.h"
#include "render/creature/spec.h"
#include "render/draw_queue.h"
#include "render/horse/horse_renderer_base.h"
#include "render/horse/horse_spec.h"
#include "render/rigged_mesh.h"
#include "render/rigged_mesh_cache.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>
#include <gtest/gtest.h>

namespace {

using Render::GL::DrawPartCmdIndex;
using Render::GL::MeshCmdIndex;
using Render::GL::RiggedCreatureCmd;
using Render::GL::RiggedCreatureCmdIndex;

Render::GL::HorseVariant make_variant() {
  Render::GL::HorseVariant v{};
  v.coat_color = QVector3D(0.42F, 0.28F, 0.18F);
  v.mane_color = QVector3D(0.10F, 0.08F, 0.06F);
  v.muzzle_color = QVector3D(0.20F, 0.15F, 0.12F);
  v.hoof_color = QVector3D(0.08F, 0.07F, 0.06F);
  return v;
}

Render::Horse::HorseSpecPose baseline_pose() {
  Render::GL::HorseDimensions const dims =
      Render::GL::make_horse_dimensions(0U);
  Render::GL::HorseGait gait{};
  Render::Horse::HorseSpecPose pose{};
  Render::Horse::make_horse_spec_pose_reduced(
      dims, gait, Render::Horse::HorseReducedMotion{}, pose);
  return pose;
}

TEST(HorseFullSwitchover, FullLodBakesMoreGeometryThanReduced) {
  auto const &spec = Render::Horse::horse_creature_spec();
  auto bind = Render::Horse::horse_bind_palette();
  EXPECT_EQ(bind.size(), Render::Horse::kHorseBoneCount);

  ASSERT_GT(spec.lod_full.primitives.size(), 0U);
  ASSERT_GT(spec.lod_reduced.primitives.size(), 0U);
  EXPECT_EQ(spec.lod_full.primitives.size(), 43U);
  EXPECT_EQ(spec.lod_reduced.primitives.size(), 9U);

  Render::GL::RiggedMeshCache cache;
  const auto *full_entry =
      cache.get_or_bake(spec, Render::Creature::CreatureLOD::Full, bind);
  const auto *reduced_entry =
      cache.get_or_bake(spec, Render::Creature::CreatureLOD::Reduced, bind);
  ASSERT_NE(full_entry, nullptr);
  ASSERT_NE(reduced_entry, nullptr);
  ASSERT_NE(full_entry->mesh, nullptr);
  ASSERT_NE(reduced_entry->mesh, nullptr);
  EXPECT_GT(full_entry->mesh->vertex_count(), 0U);
  EXPECT_GT(full_entry->mesh->vertex_count(),
            reduced_entry->mesh->vertex_count());
}

TEST(HorseFullSwitchover, BonePaletteEvaluationIsFinite) {
  auto pose = baseline_pose();
  std::array<QMatrix4x4, Render::Horse::kHorseBoneCount> buf{};
  std::uint32_t const n = Render::Horse::compute_horse_bone_palette(
      pose, std::span<QMatrix4x4>(buf.data(), buf.size()));
  EXPECT_EQ(n, Render::Horse::kHorseBoneCount);
  for (std::uint32_t i = 0; i < n; ++i) {
    for (int r = 0; r < 4; ++r) {
      for (int c = 0; c < 4; ++c) {
        EXPECT_TRUE(std::isfinite(buf[i](r, c)))
            << "horse bone " << i << " [" << r << "," << c << "]";
      }
    }
  }
}

TEST(HorseFullSwitchover, RiggedCmdEmittedForFullLod) {
  auto const &spec = Render::Horse::horse_creature_spec();
  auto bind = Render::Horse::horse_bind_palette();

  Render::GL::RiggedMeshCache cache;
  Render::GL::BonePaletteArena arena;
  const auto *entry =
      cache.get_or_bake(spec, Render::Creature::CreatureLOD::Full, bind);
  ASSERT_NE(entry, nullptr);

  auto pose = baseline_pose();
  std::array<QMatrix4x4, Render::Horse::kHorseBoneCount> palette_buf{};
  Render::Horse::compute_horse_bone_palette(
      pose, std::span<QMatrix4x4>(palette_buf.data(), palette_buf.size()));

  auto slot_h = arena.allocate_palette();
  QMatrix4x4 *slot = slot_h.cpu;
  ASSERT_NE(slot, nullptr);
  for (std::size_t i = 0; i < entry->inverse_bind.size(); ++i) {
    slot[i] = palette_buf[i] * entry->inverse_bind[i];
  }

  Render::GL::DrawQueue queue;
  Render::GL::QueueSubmitter sub(&queue);

  RiggedCreatureCmd cmd{};
  cmd.mesh = entry->mesh.get();
  cmd.bone_palette = slot;
  cmd.bone_count = static_cast<std::uint32_t>(entry->inverse_bind.size());
  cmd.color = make_variant().coat_color;
  cmd.alpha = 1.0F;
  cmd.variation_scale = QVector3D(1.0F, 1.0F, 1.0F);
  sub.rigged(cmd);

  ASSERT_EQ(queue.size(), 1U);
  EXPECT_EQ(queue.items()[0].index(), RiggedCreatureCmdIndex);

  std::size_t parts = 0, meshes = 0;
  for (const auto &c : queue.items()) {
    if (c.index() == DrawPartCmdIndex)
      ++parts;
    else if (c.index() == MeshCmdIndex)
      ++meshes;
  }
  EXPECT_EQ(parts, 0U) << "horse body emits zero DrawPartCmds in rigged path";
  EXPECT_EQ(meshes, 0U);
}

TEST(HorseFullSwitchover, VariationScaleSharedMeshDifferentUnits) {
  // Two units with different variation_scale must hit the SAME baked
  // mesh pointer (proving the variation knob doesn't trigger a re-bake)
  // while still carrying distinct per-cmd variation_scale values.
  auto const &spec = Render::Horse::horse_creature_spec();
  auto bind = Render::Horse::horse_bind_palette();

  Render::GL::RiggedMeshCache cache;
  Render::GL::BonePaletteArena arena;
  const auto *entry = cache.get_or_bake(
      spec, Render::Creature::CreatureLOD::Reduced, bind, /*variant_bucket=*/0);
  ASSERT_NE(entry, nullptr);

  Render::GL::DrawQueue queue;
  Render::GL::QueueSubmitter sub(&queue);

  for (int unit = 0; unit < 2; ++unit) {
    auto slot_h = arena.allocate_palette();
    QMatrix4x4 *slot = slot_h.cpu;
    for (std::size_t i = 0; i < entry->inverse_bind.size(); ++i) {
      slot[i] = entry->inverse_bind[i].inverted() * entry->inverse_bind[i];
    }
    RiggedCreatureCmd cmd{};
    cmd.mesh = entry->mesh.get();
    cmd.bone_palette = slot;
    cmd.bone_count = static_cast<std::uint32_t>(entry->inverse_bind.size());
    cmd.color = QVector3D(0.4F, 0.3F, 0.2F);
    cmd.alpha = 1.0F;
    cmd.variation_scale = (unit == 0) ? QVector3D(1.0F, 1.0F, 1.0F)
                                      : QVector3D(1.10F, 0.95F, 1.05F);
    sub.rigged(cmd);
  }

  ASSERT_EQ(queue.size(), 2U);
  const auto &c0 = std::get<RiggedCreatureCmdIndex>(queue.items()[0]);
  const auto &c1 = std::get<RiggedCreatureCmdIndex>(queue.items()[1]);
  EXPECT_EQ(c0.mesh, c1.mesh) << "shared baked mesh — no re-bake on variation";
  EXPECT_NE(c0.variation_scale, c1.variation_scale);
}

TEST(HorseFullSwitchover, ReducedAndFullShareBindPalette) {
  // The same bind palette feeds both LOD bakes and the cache must
  // distinguish them — proving the rigged path is wired symmetrically
  // to humanoid 15.5d.
  auto const &spec = Render::Horse::horse_creature_spec();
  auto bind = Render::Horse::horse_bind_palette();

  Render::GL::RiggedMeshCache cache;
  const auto *full =
      cache.get_or_bake(spec, Render::Creature::CreatureLOD::Full, bind);
  const auto *reduced =
      cache.get_or_bake(spec, Render::Creature::CreatureLOD::Reduced, bind);
  ASSERT_NE(full, nullptr);
  ASSERT_NE(reduced, nullptr);
  EXPECT_NE(full->mesh.get(), reduced->mesh.get());
  EXPECT_EQ(cache.size(), 2U);
}

} // namespace

namespace more {

TEST(HorseReducedSwitchover, ReducedBakeNonEmpty) {
  auto const &spec = Render::Horse::horse_creature_spec();
  auto bind = Render::Horse::horse_bind_palette();
  Render::GL::RiggedMeshCache cache;
  const auto *e =
      cache.get_or_bake(spec, Render::Creature::CreatureLOD::Reduced, bind);
  ASSERT_NE(e, nullptr);
  ASSERT_NE(e->mesh, nullptr);
  EXPECT_GT(e->mesh->vertex_count(), 0U);
  EXPECT_EQ(e->inverse_bind.size(), Render::Horse::kHorseBoneCount);
}

TEST(HorseReducedSwitchover, BindPaletteIsCached) {
  // Two calls to horse_bind_palette() must return the same backing
  // memory — proving the lazy-init static cache is stable.
  auto a = Render::Horse::horse_bind_palette();
  auto b = Render::Horse::horse_bind_palette();
  EXPECT_EQ(a.data(), b.data());
}

TEST(HorseReducedSwitchover, SpecTopologyHasExpectedBones) {
  auto const &spec = Render::Horse::horse_creature_spec();
  EXPECT_EQ(spec.topology.bones.size(), Render::Horse::kHorseBoneCount);
}

TEST(HorseFullSwitchover, FullPartGraphSpansAreContiguous) {
  auto const &spec = Render::Horse::horse_creature_spec();
  // Sanity check that the static k_horse_full_parts span exposed by
  // the spec matches the contracted size and that primitive 0 is the
  // first body-shell ellipsoid.
  ASSERT_EQ(spec.lod_full.primitives.size(), 43U);
  EXPECT_EQ(spec.lod_full.primitives[0].shape,
            Render::Creature::PrimitiveShape::OrientedSphere);
  EXPECT_EQ(spec.lod_full.primitives.back().shape,
            Render::Creature::PrimitiveShape::Mesh);
}

} // namespace more
