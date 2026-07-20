#include <algorithm>
#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <string_view>
#include <variant>
#include <vector>

#include "render/creature/skeleton.h"
#include "render/elephant/elephant_motion.h"
#include "render/elephant/elephant_profile_data.h"
#include "render/elephant/elephant_source_asset.h"
#include "render/elephant/elephant_spec.h"

namespace {

using Render::Creature::Quadruped::CustomMeshNode;

TEST(ElephantSourceAssetTest, ProductionTopologyAndSkinWeightsAreExact) {
  auto const& status = Render::Elephant::elephant_source_asset_status();
  ASSERT_TRUE(status.loaded) << status.error;
  EXPECT_EQ(status.vertex_count, 1464U);
  EXPECT_EQ(status.triangle_count, 760U);
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
  EXPECT_EQ(vertices, 1464U);
  EXPECT_EQ(triangles, 760U);
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

} // namespace
