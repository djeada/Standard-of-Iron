#include <algorithm>
#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <string_view>
#include <variant>
#include <vector>

#include "render/horse/horse_anatomy.h"
#include "render/horse/horse_gait.h"
#include "render/horse/horse_motion.h"
#include "render/horse/horse_profile_data.h"
#include "render/horse/horse_source_asset.h"
#include "render/horse/horse_spec.h"

namespace {

using Render::Horse::HorseAnatomyFault;

TEST(HorseAnatomyTest, CavalryProfileSatisfiesLandmarkContract) {
  auto const dims = Render::GL::make_horse_dimensions(0U);
  auto const anatomy = Render::Horse::make_horse_anatomy(dims);
  auto const validation = Render::Horse::validate_horse_anatomy(anatomy);

  EXPECT_TRUE(validation.valid());
  EXPECT_GT(anatomy.poll.y(), anatomy.withers.y());
  EXPECT_GT(anatomy.poll.z(), anatomy.neck_base.z());
  EXPECT_LT(anatomy.face_tip.y(), anatomy.poll.y());
  EXPECT_GT(anatomy.face_tip.z(), anatomy.poll.z());
  EXPECT_GT(anatomy.front_leg_root.z(), anatomy.back_center.z());
  EXPECT_LT(anatomy.rear_leg_root.z(), anatomy.back_center.z());
}

TEST(HorseAnatomyTest, ValidatorReportsIndependentSilhouetteFaults) {
  auto anatomy =
      Render::Horse::make_horse_anatomy(Render::GL::make_horse_dimensions(0U));
  anatomy.poll.setZ(anatomy.neck_base.z());
  anatomy.face_tip = anatomy.poll + QVector3D(0.0F, 0.1F, -0.1F);

  auto const validation = Render::Horse::validate_horse_anatomy(anatomy);
  EXPECT_FALSE(validation.valid());
  EXPECT_TRUE(validation.has(HorseAnatomyFault::NeckTooUpright));
  EXPECT_TRUE(validation.has(HorseAnatomyFault::FaceNotSloped));
}

TEST(HorseAnatomyTest, MountedSeatRemainsOverLoadBearingBack) {
  auto const profile = Render::GL::make_horse_profile(
      0U, QVector3D(0.4F, 0.25F, 0.12F), QVector3D(0.7F, 0.2F, 0.1F));
  auto const anatomy = Render::Horse::make_horse_anatomy(profile.dims);
  auto const mount = Render::GL::compute_mount_frame(profile);

  EXPECT_GT(mount.seat_position.z(), anatomy.croup.z());
  EXPECT_LT(mount.seat_position.z(), anatomy.withers.z());
  EXPECT_GT(mount.seat_position.y(), anatomy.back_center.y());
  EXPECT_LT(std::abs(mount.seat_position.x()), anatomy.body_width * 0.05F);
  EXPECT_LT(mount.stirrup_bottom_left.y(), mount.seat_position.y());
  EXPECT_LT(mount.stirrup_bottom_right.y(), mount.seat_position.y());
}

TEST(HorseSourceAssetTest, ProductionTopologyAndSkinWeightsAreExact) {
  auto const& status = Render::Horse::horse_source_asset_status();
  ASSERT_TRUE(status.loaded) << status.error;
  EXPECT_EQ(status.vertex_count, 4400U);
  EXPECT_EQ(status.triangle_count, 2182U);
  EXPECT_EQ(status.clip_count, 13U);
  EXPECT_EQ(Render::Horse::horse_source_bind_palette().size(), 50U);
  EXPECT_TRUE(Render::Creature::validate_topology(Render::Horse::horse_topology()));

  std::size_t vertices = 0U;
  std::size_t triangles = 0U;
  for (auto const& node : Render::Horse::horse_source_mesh_nodes()) {
    auto const* mesh =
        std::get_if<Render::Creature::Quadruped::CustomMeshNode>(&node.data);
    ASSERT_NE(mesh, nullptr);
    vertices += mesh->vertices.size();
    triangles += mesh->indices.size() / 3U;
    for (auto const& vertex : mesh->vertices) {
      float sum = 0.0F;
      for (std::size_t influence = 0U; influence < 4U; ++influence) {
        EXPECT_LT(vertex.bone_indices[influence], 50U);
        EXPECT_GE(vertex.bone_weights[influence], 0.0F);
        sum += vertex.bone_weights[influence];
      }
      EXPECT_NEAR(sum, 1.0F, 1.0e-5F);
    }
  }
  EXPECT_EQ(vertices, 4400U);
  EXPECT_EQ(triangles, 2182U);
}

TEST(HorseSourceAssetTest, ProductionBoundsUseShortenedLength) {
  EXPECT_FLOAT_EQ(Render::Horse::k_horse_mesh_scale_x, 0.59F);
  EXPECT_FLOAT_EQ(Render::Horse::k_horse_mesh_scale_y, 0.59F);
  EXPECT_FLOAT_EQ(Render::Horse::k_horse_mesh_scale_z / 0.59F, 0.85F);

  auto const bind = Render::Horse::horse_source_bind_palette();
  QVector3D bounds_min(std::numeric_limits<float>::max(),
                       std::numeric_limits<float>::max(),
                       std::numeric_limits<float>::max());
  QVector3D bounds_max(std::numeric_limits<float>::lowest(),
                       std::numeric_limits<float>::lowest(),
                       std::numeric_limits<float>::lowest());
  for (auto const& node : Render::Horse::horse_source_mesh_nodes()) {
    auto const& mesh = std::get<Render::Creature::Quadruped::CustomMeshNode>(node.data);
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
  EXPECT_NEAR(span.x(), 0.830092F, 2.0e-5F);
  EXPECT_NEAR(span.y(), 2.84613F, 2.0e-5F);
  EXPECT_NEAR(span.z(), 2.846574F, 2.0e-5F);
}

TEST(HorseSourceAssetTest, WalkAndGallopRemainFiniteWithoutTriangleTearing) {
  auto const bind = Render::Horse::horse_source_bind_palette();
  auto const nodes = Render::Horse::horse_source_mesh_nodes();
  std::array<std::string_view, 2> const clips{"Walk", "Gallop"};
  for (std::string_view const clip : clips) {
    float max_motion = 0.0F;
    for (int frame = 0; frame < 64; ++frame) {
      Render::Horse::BonePalette pose{};
      ASSERT_TRUE(Render::Horse::horse_source_sample_clip(
          clip, static_cast<float>(frame) / 64.0F, pose));
      std::array<QMatrix4x4, Render::Horse::k_horse_bone_count> delta{};
      for (std::size_t bone = 0U; bone < delta.size(); ++bone) {
        delta[bone] = pose[bone] * bind[bone].inverted();
        QVector3D const origin = pose[bone].column(3).toVector3D();
        EXPECT_TRUE(std::isfinite(origin.x()));
        EXPECT_TRUE(std::isfinite(origin.y()));
        EXPECT_TRUE(std::isfinite(origin.z()));
      }
      max_motion =
          std::max(max_motion,
                   (pose[static_cast<std::size_t>(Render::Horse::HorseBone::FootFL)]
                        .column(3)
                        .toVector3D() -
                    bind[static_cast<std::size_t>(Render::Horse::HorseBone::FootFL)]
                        .column(3)
                        .toVector3D())
                       .length());

      for (auto const& node : nodes) {
        auto const& mesh =
            std::get<Render::Creature::Quadruped::CustomMeshNode>(node.data);
        std::vector<QVector3D> deformed;
        deformed.reserve(mesh.vertices.size());
        QMatrix4x4 const root = bind[static_cast<std::size_t>(node.anchor_bone)];
        for (auto const& vertex : mesh.vertices) {
          QVector3D const local(
              vertex.position[0], vertex.position[1], vertex.position[2]);
          QVector3D const rest = root.map(local);
          QVector3D skinned{};
          for (std::size_t influence = 0U; influence < 4U; ++influence) {
            skinned += delta[vertex.bone_indices[influence]].map(rest) *
                       vertex.bone_weights[influence];
          }
          EXPECT_LT(std::abs(skinned.x()), 4.0F);
          EXPECT_LT(std::abs(skinned.y()), 4.0F);
          EXPECT_LT(std::abs(skinned.z()), 4.0F);
          deformed.push_back(skinned);
        }
        for (std::size_t index = 0U; index + 2U < mesh.indices.size(); index += 3U) {
          auto const a = mesh.indices[index];
          auto const b = mesh.indices[index + 1U];
          auto const c = mesh.indices[index + 2U];
          float const longest = std::max({(deformed[a] - deformed[b]).length(),
                                          (deformed[b] - deformed[c]).length(),
                                          (deformed[c] - deformed[a]).length()});
          EXPECT_LT(longest, 0.65F);
        }
      }
    }
    EXPECT_GT(max_motion, 0.10F);
  }
}

TEST(HorseSourceAssetTest, RiderSocketFollowsAuthoredBackBone) {
  auto const profile = Render::GL::make_horse_profile(
      0U, QVector3D(0.4F, 0.25F, 0.12F), QVector3D(0.7F, 0.2F, 0.1F));
  auto const bind = Render::Horse::horse_source_bind_palette();
  auto const back = static_cast<std::size_t>(Render::Horse::HorseBone::SourceBack);
  auto const bind_local_seat =
      bind[back].inverted().map(Render::GL::compute_mount_frame(profile).seat_position);

  for (int frame = 0; frame < 64; ++frame) {
    float const phase = static_cast<float>(frame) / 64.0F;
    auto mount = Render::GL::compute_mount_frame(profile);
    ASSERT_TRUE(Render::Horse::horse_source_pose_mount_frame("Gallop", phase, mount));
    Render::Horse::BonePalette pose{};
    ASSERT_TRUE(Render::Horse::horse_source_sample_clip("Gallop", phase, pose));
    QVector3D const expected = pose[back].map(bind_local_seat);
    EXPECT_LT((mount.seat_position - expected).length(), 1.0e-5F);
    EXPECT_NEAR(mount.seat_right.length(), 1.0F, 1.0e-5F);
    EXPECT_NEAR(mount.seat_up.length(), 1.0F, 1.0e-5F);
    EXPECT_NEAR(mount.seat_forward.length(), 1.0F, 1.0e-5F);
  }
}

} // namespace
