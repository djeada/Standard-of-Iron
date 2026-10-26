#include <algorithm>
#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <span>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "animation/elephant_gait_manifest.h"
#include "animation/elephant_motion_manifest.h"
#include "render/creature/schema/creature_runtime_manifest.h"
#include "render/creature/skeleton.h"
#include "render/elephant/elephant_manifest.h"
#include "render/elephant/elephant_motion.h"
#include "render/elephant/elephant_profile_data.h"
#include "render/elephant/elephant_source_asset.h"
#include "render/elephant/elephant_spec.h"

namespace {

using Render::Creature::Quadruped::CustomMeshNode;

TEST(ElephantSourceAssetTest, SwingLiftEasesIntoContactAndLeavesStanceFeetLevel) {
  for (auto const* shape :
       {&Animation::elephant_walk_shape(), &Animation::elephant_run_shape()}) {
    for (auto const leg : {Animation::ElephantLeg::FrontLeft,
                           Animation::ElephantLeg::FrontRight,
                           Animation::ElephantLeg::BackLeft,
                           Animation::ElephantLeg::BackRight}) {
      float const offset = Animation::elephant_leg_phase_offset_lateral(leg);
      constexpr float step = 0.0001F;
      auto const lifting =
          Animation::sample_elephant_leg(*shape, leg, offset + shape->duty + step);
      auto const landing =
          Animation::sample_elephant_leg(*shape, leg, offset + 1.0F - step);

      EXPECT_LT(std::abs(lifting.knee_degrees) / step, 1.0F);
      EXPECT_LT(std::abs(landing.knee_degrees) / step, 1.0F);
      auto const mid_swing = Animation::sample_elephant_leg(
          *shape, leg, offset + shape->duty + (1.0F - shape->duty) * 0.5F);
      EXPECT_NEAR(mid_swing.knee_degrees, shape->lift_knee_degrees, 0.001F);
      for (int frame = 0; frame < 100; ++frame) {
        auto const sample = Animation::sample_elephant_leg(
            *shape, leg, offset + shape->duty * frame / 100.0F);
        EXPECT_NEAR(sample.thigh_degrees + sample.knee_degrees + sample.foot_degrees,
                    0.0F,
                    0.001F);
      }
    }
  }
}

TEST(ElephantSourceAssetTest, SecondaryMotionLoopsWithRestrainedBodyRotation) {
  for (auto const* shape :
       {&Animation::elephant_walk_shape(), &Animation::elephant_run_shape()}) {
    for (int frame = 0; frame < 128; ++frame) {
      float const phase = frame / 128.0F;
      auto const a = Animation::elephant_locomotion_secondary(*shape, phase);
      auto const b = Animation::elephant_locomotion_secondary(*shape, phase + 1.0F);
      EXPECT_LE(std::abs(a.body_roll_degrees), 1.0F);
      EXPECT_LE(std::abs(a.body_pitch_degrees), 0.3F);
      EXPECT_NEAR(a.body_roll_degrees, b.body_roll_degrees, 0.0001F);
      EXPECT_NEAR(a.ear_left_degrees, b.ear_left_degrees, 0.0001F);
      EXPECT_NEAR(a.ear_right_degrees, b.ear_right_degrees, 0.0001F);
      EXPECT_LE(std::abs(a.ear_left_degrees), shape->ear_flap_degrees);
      EXPECT_LE(std::abs(a.ear_right_degrees), shape->ear_flap_degrees);
    }
  }
}

TEST(ElephantSourceAssetTest, ProductionTopologyAndSkinWeightsAreExact) {
  auto const& status = Render::Elephant::elephant_source_asset_status();
  ASSERT_TRUE(status.loaded) << status.error;
  EXPECT_EQ(status.vertex_count, 1474U);
  EXPECT_EQ(status.triangle_count, 772U);
  EXPECT_EQ(status.clip_count, 5U);
  EXPECT_EQ(Render::Elephant::elephant_source_bind_palette().size(), 32U);
  EXPECT_TRUE(
      Render::Creature::validate_topology(Render::Elephant::elephant_topology()));

  std::size_t vertices = 0U;
  std::size_t triangles = 0U;
  for (auto const& node : Render::Elephant::elephant_source_mesh_nodes()) {
    auto const* mesh = std::get_if<CustomMeshNode>(&node.data);
    ASSERT_NE(mesh, nullptr);
    vertices += mesh->vertices.size();
    triangles += mesh->indices.size() / 3U;
    for (auto const& vertex : mesh->vertices) {
      float weight_sum = 0.0F;
      for (std::size_t influence = 0U; influence < 4U; ++influence) {
        EXPECT_LT(vertex.bone_indices[influence], 32U);
        EXPECT_GE(vertex.bone_weights[influence], 0.0F);
        weight_sum += vertex.bone_weights[influence];
      }
      EXPECT_NEAR(weight_sum, 1.0F, 1.0e-5F);
    }
  }
  EXPECT_EQ(vertices, 1474U);
  EXPECT_EQ(triangles, 772U);
}

TEST(ElephantSourceAssetTest, ProductionMeshCarriesBothEyes) {
  auto const bind = Render::Elephant::elephant_source_bind_palette();
  bool found_eyes = false;
  for (auto const& node : Render::Elephant::elephant_source_mesh_nodes()) {
    if (node.debug_name != "elephant.production.eyes") {
      continue;
    }
    found_eyes = true;
    auto const& mesh = std::get<CustomMeshNode>(node.data);
    QMatrix4x4 const root = bind[static_cast<std::size_t>(node.anchor_bone)];

    bool left = false;
    bool right = false;
    for (auto const& vertex : mesh.vertices) {
      QVector3D const rest = root.map(
          QVector3D(vertex.position[0], vertex.position[1], vertex.position[2]));

      if (rest.z() <= 0.0F) {
        continue;
      }
      left = left || rest.x() < -0.05F;
      right = right || rest.x() > 0.05F;
    }
    EXPECT_TRUE(left) << "elephant is missing its left eye";
    EXPECT_TRUE(right) << "elephant is missing its right eye";
  }
  EXPECT_TRUE(found_eyes);
}

TEST(ElephantSourceAssetTest, FullLodRefinesTheSkinAndMinimalKeepsTheCage) {
  auto const source = Render::Elephant::elephant_source_mesh_nodes();
  auto const& manifest = Render::Elephant::elephant_runtime_manifest();
  ASSERT_EQ(manifest.lod_full.mesh_nodes.size(), source.size());
  ASSERT_EQ(manifest.lod_minimal.mesh_nodes.size(), source.size());
  for (std::size_t index = 0U; index < source.size(); ++index) {
    auto const source_triangles =
        std::get<CustomMeshNode>(source[index].data).indices.size() / 3U;
    auto const full_triangles =
        std::get<CustomMeshNode>(manifest.lod_full.mesh_nodes[index].data)
            .indices.size() /
        3U;
    auto const minimal_triangles =
        std::get<CustomMeshNode>(manifest.lod_minimal.mesh_nodes[index].data)
            .indices.size() /
        3U;
    EXPECT_EQ(manifest.lod_full.mesh_nodes[index].debug_name, source[index].debug_name);
    EXPECT_EQ(minimal_triangles, source_triangles);
    if (source[index].debug_name == "elephant.production.skin") {
      EXPECT_GE(full_triangles, source_triangles * 15U);
    } else {
      EXPECT_EQ(full_triangles, source_triangles);
    }
  }
}

TEST(ElephantSourceAssetTest, RefinedSkinKeepsWeightsGroundAndSilhouette) {
  auto const bind = Render::Elephant::elephant_source_bind_palette();
  auto const& manifest = Render::Elephant::elephant_runtime_manifest();
  auto bounds = [&](std::span<const Render::Creature::Quadruped::MeshNode> nodes) {
    QVector3D lo(std::numeric_limits<float>::max(),
                 std::numeric_limits<float>::max(),
                 std::numeric_limits<float>::max());
    QVector3D hi(std::numeric_limits<float>::lowest(),
                 std::numeric_limits<float>::lowest(),
                 std::numeric_limits<float>::lowest());
    for (auto const& node : nodes) {
      auto const& mesh = std::get<CustomMeshNode>(node.data);
      QMatrix4x4 const root = bind[static_cast<std::size_t>(node.anchor_bone)];
      for (auto const& vertex : mesh.vertices) {
        float weight_sum = 0.0F;
        for (float const weight : vertex.bone_weights) {
          weight_sum += weight;
        }
        EXPECT_NEAR(weight_sum, 1.0F, 1.0e-4F);
        QVector3D const point = root.map(
            QVector3D(vertex.position[0], vertex.position[1], vertex.position[2]));
        for (int axis = 0; axis < 3; ++axis) {
          lo[axis] = std::min(lo[axis], point[axis]);
          hi[axis] = std::max(hi[axis], point[axis]);
        }
      }
    }
    return std::make_pair(lo, hi);
  };
  auto const [full_lo, full_hi] = bounds(manifest.lod_full.mesh_nodes);
  auto const [cage_lo, cage_hi] = bounds(manifest.lod_minimal.mesh_nodes);
  EXPECT_NEAR(full_lo.y(), cage_lo.y(), 0.01F) << "the soles must stay on the ground";
  for (int axis = 0; axis < 3; ++axis) {
    EXPECT_NEAR(full_hi[axis] - full_lo[axis], cage_hi[axis] - cage_lo[axis], 0.08F)
        << "the refined silhouette drifted from the cage on axis " << axis;
  }
}

TEST(ElephantSourceAssetTest, AttackLoadsCommitsAndSettlesWithoutPhaseJumps) {
  using Animation::elephant_attack_source_phase;
  EXPECT_FLOAT_EQ(elephant_attack_source_phase(0.0F), 0.0F);
  EXPECT_FLOAT_EQ(elephant_attack_source_phase(1.0F), 1.0F);
  EXPECT_LT(elephant_attack_source_phase(0.28F), 0.28F);
  EXPECT_GT(elephant_attack_source_phase(0.58F), 0.58F);
  float previous = 0.0F;
  for (int frame = 1; frame <= 240; ++frame) {
    float const phase = elephant_attack_source_phase(frame / 240.0F);
    EXPECT_GE(phase, previous);
    EXPECT_LT(phase - previous, 0.015F);
    EXPECT_LE(phase, 1.0F);
    previous = phase;
  }
}

TEST(ElephantSourceAssetTest, ProductionBoundsUseWidenedHalfSizeAndShortenedLength) {
  EXPECT_FLOAT_EQ(Render::Elephant::k_elephant_mesh_scale_x / 0.55F, 0.575F);
  EXPECT_FLOAT_EQ(Render::Elephant::k_elephant_mesh_scale_y / 0.55F, 0.5F);
  EXPECT_FLOAT_EQ(Render::Elephant::k_elephant_mesh_scale_z / 0.55F, 0.35F);

  auto const bind = Render::Elephant::elephant_source_bind_palette();
  QVector3D bounds_min(std::numeric_limits<float>::max(),
                       std::numeric_limits<float>::max(),
                       std::numeric_limits<float>::max());
  QVector3D bounds_max(std::numeric_limits<float>::lowest(),
                       std::numeric_limits<float>::lowest(),
                       std::numeric_limits<float>::lowest());
  for (auto const& node : Render::Elephant::elephant_source_mesh_nodes()) {
    auto const& mesh = std::get<CustomMeshNode>(node.data);
    QMatrix4x4 const root = bind[static_cast<std::size_t>(node.anchor_bone)];
    for (auto const& vertex : mesh.vertices) {
      QVector3D const point = root.map(
          QVector3D(vertex.position[0], vertex.position[1], vertex.position[2]));
      for (int axis = 0; axis < 3; ++axis) {
        bounds_min[axis] = std::min(bounds_min[axis], point[axis]);
        bounds_max[axis] = std::max(bounds_max[axis], point[axis]);
      }
    }
  }

  QVector3D const span = bounds_max - bounds_min;
  EXPECT_NEAR(bounds_min.y(), 0.0F, 2.0e-6F);
  EXPECT_NEAR(span.x(), 1.342232F, 2.0e-5F);
  EXPECT_NEAR(span.y(), 1.73836F, 2.0e-5F);
  EXPECT_NEAR(span.z(), 2.19962F, 2.0e-5F);
}

TEST(ElephantSourceAssetTest, WalkAndFastWalkRemainFiniteWithoutTearing) {
  auto const bind = Render::Elephant::elephant_source_bind_palette();
  std::array<std::string_view, 2> const clips{"Walk", "Run"};
  for (std::string_view const clip : clips) {
    float max_foot_motion = 0.0F;
    for (int frame = 0; frame < 64; ++frame) {
      Render::Elephant::BonePalette pose{};
      ASSERT_TRUE(Render::Elephant::elephant_source_sample_clip(
          clip, static_cast<float>(frame) / 64.0F, pose));
      std::array<QMatrix4x4, Render::Elephant::k_elephant_bone_count> delta{};
      for (std::size_t bone = 0U; bone < delta.size(); ++bone) {
        delta[bone] = pose[bone] * bind[bone].inverted();
        QVector3D const origin = pose[bone].column(3).toVector3D();
        EXPECT_TRUE(std::isfinite(origin.x()));
        EXPECT_TRUE(std::isfinite(origin.y()));
        EXPECT_TRUE(std::isfinite(origin.z()));
      }
      auto const foot =
          static_cast<std::size_t>(Render::Elephant::ElephantBone::FootFL);
      max_foot_motion = std::max(
          max_foot_motion,
          (pose[foot].column(3).toVector3D() - bind[foot].column(3).toVector3D())
              .length());

      for (auto const& node : Render::Elephant::elephant_source_mesh_nodes()) {
        auto const& mesh = std::get<CustomMeshNode>(node.data);
        QMatrix4x4 const root = bind[static_cast<std::size_t>(node.anchor_bone)];
        std::vector<QVector3D> deformed;
        deformed.reserve(mesh.vertices.size());
        for (auto const& vertex : mesh.vertices) {
          QVector3D const rest = root.map(
              QVector3D(vertex.position[0], vertex.position[1], vertex.position[2]));
          QVector3D skinned{};
          for (std::size_t influence = 0U; influence < 4U; ++influence) {
            skinned += delta[vertex.bone_indices[influence]].map(rest) *
                       vertex.bone_weights[influence];
          }
          EXPECT_LT(std::abs(skinned.x()), 8.0F);
          EXPECT_LT(std::abs(skinned.y()), 8.0F);
          EXPECT_LT(std::abs(skinned.z()), 8.0F);
          deformed.push_back(skinned);
        }
        for (std::size_t i = 0U; i + 2U < mesh.indices.size(); i += 3U) {
          auto const a = mesh.indices[i];
          auto const b = mesh.indices[i + 1U];
          auto const c = mesh.indices[i + 2U];
          float const longest = std::max({(deformed[a] - deformed[b]).length(),
                                          (deformed[b] - deformed[c]).length(),
                                          (deformed[c] - deformed[a]).length()});
          EXPECT_LT(longest, 2.0F);
        }
      }
    }
    EXPECT_GT(max_foot_motion, 0.15F);
  }
}

TEST(ElephantSourceAssetTest, HowdahSocketFollowsAuthoredBackBone) {
  auto const profile = Render::GL::make_elephant_profile(
      0U, QVector3D(0.7F, 0.2F, 0.1F), QVector3D(0.8F, 0.7F, 0.4F));
  auto const bind = Render::Elephant::elephant_source_bind_palette();
  auto const base_frame = Render::GL::compute_howdah_frame(profile);
  EXPECT_FLOAT_EQ(base_frame.howdah_center.y(), 1.75F);
  EXPECT_FLOAT_EQ(base_frame.howdah_center.z(), -0.0875F);
  EXPECT_FLOAT_EQ(base_frame.seat_position.y(), 1.94F);
  constexpr std::size_t k_back = 1U;
  QVector3D const local = bind[k_back].inverted().map(base_frame.seat_position);
  for (int frame = 0; frame < 64; ++frame) {
    float const phase = static_cast<float>(frame) / 64.0F;
    auto howdah = Render::GL::compute_howdah_frame(profile);
    ASSERT_TRUE(Render::Elephant::elephant_source_pose_howdah("Run", phase, howdah));
    Render::Elephant::BonePalette pose{};
    ASSERT_TRUE(Render::Elephant::elephant_source_sample_clip("Run", phase, pose));
    EXPECT_LT((howdah.seat_position - pose[k_back].map(local)).length(), 1.0e-5F);
    EXPECT_NEAR(howdah.seat_up.length(), 1.0F, 1.0e-5F);
  }
}

TEST(ElephantSourceAssetTest, SynthesisedGaitKeepsTheLegsUnderTheBody) {
  auto const bind = Render::Elephant::elephant_source_bind_palette();
  ASSERT_EQ(bind.size(), 32U);

  constexpr std::size_t k_foot_front_left = 11U;
  constexpr std::size_t k_foot_front_right = 14U;
  constexpr std::size_t k_foot_rear_left = 17U;
  constexpr std::size_t k_foot_rear_right = 20U;
  constexpr std::size_t k_hip_front_left = 9U;

  float const leg_length = (bind[k_hip_front_left].column(3).toVector3D().y() -
                            bind[k_foot_front_left].column(3).toVector3D().y());
  ASSERT_GT(leg_length, 0.1F);

  for (auto const* clip : {"Walk", "Run"}) {
    float widest_pair = 0.0F;
    float furthest_foot = 0.0F;
    for (int frame = 0; frame < 64; ++frame) {
      float const phase = static_cast<float>(frame) / 64.0F;
      Render::Elephant::BonePalette pose{};
      ASSERT_TRUE(Render::Elephant::elephant_source_sample_clip(clip, phase, pose))
          << clip;

      for (auto const& pair : {std::pair{k_foot_front_left, k_foot_front_right},
                               std::pair{k_foot_rear_left, k_foot_rear_right}}) {
        float const spread = std::abs(pose[pair.first].column(3).toVector3D().z() -
                                      pose[pair.second].column(3).toVector3D().z());
        widest_pair = std::max(widest_pair, spread);
      }
      for (auto const foot : {k_foot_front_left,
                              k_foot_front_right,
                              k_foot_rear_left,
                              k_foot_rear_right}) {
        float const travel = std::abs(pose[foot].column(3).toVector3D().z() -
                                      bind[foot].column(3).toVector3D().z());
        furthest_foot = std::max(furthest_foot, travel);
      }
    }

    EXPECT_LT(widest_pair, leg_length * 0.75F) << clip << " is doing the splits";
    EXPECT_LT(furthest_foot, leg_length * 0.42F) << clip << " over-strides";

    EXPECT_GT(furthest_foot, leg_length * 0.2F) << clip << " does not animate";
  }
}

} // namespace
