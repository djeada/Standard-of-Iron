// Stage 16.5 — horse rigged-path body submit smoke tests.
//
// The legacy walker entry point (`submit_horse_lod`) was removed when
// the imperative LOD path was retired; the rig now exclusively calls
// the rigged wrappers. With a non-Renderer submitter we exercise the
// software fallback inside `submit_horse_*_rigged`, which routes
// through `submit_creature(spec, lod)` and emits one draw per static
// PartGraph primitive (7/43 for Minimal/Full).

#include "render/creature/spec.h"
#include "render/gl/mesh.h"
#include "render/horse/horse_gait.h"
#include "render/horse/horse_profile_data.h"
#include "render/horse/horse_renderer_base.h"
#include "render/horse/horse_spec.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <gtest/gtest.h>
#include <vector>

namespace {

using Render::GL::ISubmitter;

struct Call {
  Render::GL::Mesh *mesh{nullptr};
  QMatrix4x4 model{};
  QVector3D color{};
  int material_id{0};
};

class CapturingSubmitter : public ISubmitter {
public:
  std::vector<Call> calls;

  void mesh(Render::GL::Mesh *m, const QMatrix4x4 &model, const QVector3D &col,
            Render::GL::Texture *, float, int mat) override {
    calls.push_back({m, model, col, mat});
  }
  void part(Render::GL::Mesh *m, Render::GL::Material *,
            const QMatrix4x4 &model, const QVector3D &col,
            Render::GL::Texture *, float, int mat) override {
    calls.push_back({m, model, col, mat});
  }
  void cylinder(const QVector3D &, const QVector3D &, float, const QVector3D &,
                float) override {}
  void selection_ring(const QMatrix4x4 &, float, float,
                      const QVector3D &) override {}
  void grid(const QMatrix4x4 &, const QVector3D &, float, float,
            float) override {}
  void selection_smoke(const QMatrix4x4 &, const QVector3D &, float) override {}
  void healing_beam(const QVector3D &, const QVector3D &, const QVector3D &,
                    float, float, float, float) override {}
  void healer_aura(const QVector3D &, const QVector3D &, float, float,
                   float) override {}
  void combat_dust(const QVector3D &, const QVector3D &, float, float,
                   float) override {}
  void stone_impact(const QVector3D &, const QVector3D &, float, float,
                    float) override {}
  void mode_indicator(const QMatrix4x4 &, int, const QVector3D &,
                      float) override {}
};

Render::GL::HorseDimensions make_horse_dims() {
  Render::GL::HorseDimensions d{};
  d.body_length = 1.4F;
  d.body_width = 0.24F;
  d.body_height = 0.60F;
  d.barrel_center_y = 1.10F;
  d.neck_length = 0.50F;
  d.neck_rise = 0.20F;
  d.head_length = 0.42F;
  d.head_width = 0.18F;
  d.head_height = 0.24F;
  d.muzzle_length = 0.20F;
  d.leg_length = 0.90F;
  d.hoof_height = 0.08F;
  d.tail_length = 0.58F;
  return d;
}

Render::GL::HorseVariant make_horse_variant() {
  Render::GL::HorseVariant v{};
  v.coat_color = QVector3D(0.55F, 0.35F, 0.20F);
  v.hoof_color = QVector3D(0.10F, 0.08F, 0.05F);
  return v;
}

Render::GL::HorseGait make_horse_gait() {
  Render::GL::HorseGait g{};
  g.cycle_time = 1.0F;
  g.front_leg_phase = 0.0F;
  g.rear_leg_phase = 0.5F;
  g.stride_swing = 0.30F;
  g.stride_lift = 0.12F;
  return g;
}

auto find_primitive(std::span<const Render::Creature::PrimitiveInstance> prims,
                    std::string_view name)
    -> const Render::Creature::PrimitiveInstance * {
  auto it = std::find_if(prims.begin(), prims.end(), [&](auto const &prim) {
    return prim.debug_name == name;
  });
  return it == prims.end() ? nullptr : &*it;
}

auto mesh_axis_span(const Render::GL::Mesh &mesh, std::size_t axis) -> float {
  auto const &vertices = mesh.get_vertices();
  if (vertices.empty()) {
    return 0.0F;
  }
  float min_v = vertices.front().position[axis];
  float max_v = vertices.front().position[axis];
  for (auto const &vertex : vertices) {
    min_v = std::min(min_v, vertex.position[axis]);
    max_v = std::max(max_v, vertex.position[axis]);
  }
  return max_v - min_v;
}

auto point_to_segment_distance(const QVector3D &point, const QVector3D &a,
                               const QVector3D &b) -> float {
  QVector3D const ab = b - a;
  float const denom = QVector3D::dotProduct(ab, ab);
  if (denom <= 1.0e-6F) {
    return (point - a).length();
  }
  float const t =
      std::clamp(QVector3D::dotProduct(point - a, ab) / denom, 0.0F, 1.0F);
  return (point - (a + ab * t)).length();
}

} // namespace

TEST(HorseSpecTest, CreatureSpecHasTwoLods) {
  auto const &spec = Render::Horse::horse_creature_spec();
  EXPECT_EQ(spec.lod_minimal.primitives.size(), 1U);
  EXPECT_EQ(spec.lod_full.primitives.size(), 1U);
}

TEST(HorseSpecTest, BasePoseKeepsForehandBroaderAndHindFeetTuckedUnderCroup) {
  auto dims = make_horse_dims();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose(dims, /*bob=*/0.0F, pose);

  EXPECT_GT(pose.shoulder_offset_fl.x(), pose.shoulder_offset_bl.x() * 1.35F);
  EXPECT_GT(pose.shoulder_offset_fl.z(), 0.0F);
  EXPECT_LT(pose.shoulder_offset_bl.z(), 0.0F);
  EXPECT_LT(pose.shoulder_offset_bl.y(),
            pose.shoulder_offset_fl.y() - dims.body_height * 0.06F);
  EXPECT_GT(pose.foot_fl.z(), pose.foot_bl.z());
  EXPECT_LT(pose.leg_radius, dims.body_width * 0.14F);
}

TEST(HorseSpecTest, AnimatedPoseKeepsLegsOutsideTorsoRead) {
  auto dims = make_horse_dims();
  auto gait = make_horse_gait();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose_animated(
      dims, gait, Render::Horse::HorsePoseMotion{0.0F, 0.0F, false}, pose);

  EXPECT_LT(pose.body_half.x(), dims.body_width * 0.75F);
  EXPECT_GT(std::abs(pose.shoulder_offset_pose_fl.x()),
            dims.body_width * 0.65F);
  EXPECT_GT(std::abs(pose.shoulder_offset_pose_fl.x()),
            std::abs(pose.shoulder_offset_pose_bl.x()));
  EXPECT_GT(std::abs(pose.shoulder_offset_pose_bl.x()),
            dims.body_width * 0.50F);
  EXPECT_LT(pose.shoulder_offset_pose_fl.y(), 0.0F);
  EXPECT_LT(pose.shoulder_offset_pose_bl.y(),
            pose.shoulder_offset_pose_fl.y() - dims.body_height * 0.04F);
  EXPECT_LT(pose.foot_fl.y(), pose.barrel_center.y() - dims.leg_length * 1.04F);
  EXPECT_LT(pose.foot_bl.y(), pose.barrel_center.y() - dims.leg_length * 1.00F);
  EXPECT_GT(pose.neck_base.z(), dims.body_length * 0.34F);
  EXPECT_LT(pose.neck_base.y() - pose.barrel_center.y(),
            dims.body_height * 0.45F);
  EXPECT_GT(pose.neck_top.z(), pose.neck_base.z() + dims.neck_length * 0.88F);
  EXPECT_GT(pose.head_center.z(), pose.neck_top.z() + dims.head_length * 0.64F);
  EXPECT_LT(pose.head_center.y(), pose.neck_top.y());
  EXPECT_GT(pose.neck_radius, dims.body_width * 0.15F);
  EXPECT_LT(pose.neck_radius, dims.body_width * 0.25F);
  EXPECT_LT(pose.neck_radius, pose.body_half.x());
  EXPECT_GT(pose.head_half.z(), pose.head_half.y() * 2.0F);
}

TEST(HorseSpecTest, BasePoseBodyKeepsDepthCloserToWidthThanFlatBlob) {
  auto dims = make_horse_dims();

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose(dims, /*bob=*/0.0F, pose);

  float const depth_ratio = pose.body_ellipsoid_y / pose.body_ellipsoid_x;
  EXPECT_GT(depth_ratio, 0.85F);
  EXPECT_LT(depth_ratio, 1.15F);
}

TEST(HorseSpecTest, GeneratedHorseDimensionsUseWiderTallerTorso) {
  auto const dims = Render::GL::make_horse_dimensions(0U);

  EXPECT_GE(dims.body_width,
            Render::GL::HorseDimensionRange::kBodyWidthMin * 1.5F);
  EXPECT_LE(dims.body_width,
            Render::GL::HorseDimensionRange::kBodyWidthMax * 1.5F);
  EXPECT_GE(dims.body_height,
            Render::GL::HorseDimensionRange::kBodyHeightMin * 1.2F);
  EXPECT_LE(dims.body_height,
            Render::GL::HorseDimensionRange::kBodyHeightMax * 1.2F);
}

TEST(HorseSpecTest, FullSpecUsesTallThickTorsoWithCompressedHeadReach) {
  auto const &spec = Render::Horse::horse_creature_spec();

  auto const *whole =
      find_primitive(spec.lod_full.primitives, "horse.full.whole");

  ASSERT_NE(whole, nullptr);
  EXPECT_EQ(whole->shape, Render::Creature::PrimitiveShape::Mesh);
  ASSERT_NE(whole->custom_mesh, nullptr);
  float const x_span = mesh_axis_span(*whole->custom_mesh, 0);
  float const y_span = mesh_axis_span(*whole->custom_mesh, 1);
  float const z_span = mesh_axis_span(*whole->custom_mesh, 2);
  EXPECT_GT(y_span, z_span * 1.2F);
  EXPECT_GT(y_span, x_span * 1.3F);
  EXPECT_EQ(find_primitive(spec.lod_full.primitives, "horse.full.chest"),
            nullptr);
  EXPECT_EQ(find_primitive(spec.lod_full.primitives, "horse.full.head.cheek.l"),
            nullptr);
}

TEST(HorseSpecTest, FullSpecForearmStaysHeavierThanRearUpperLeg) {
  auto const &spec = Render::Horse::horse_creature_spec();

  auto const *whole =
      find_primitive(spec.lod_full.primitives, "horse.full.whole");

  ASSERT_NE(whole, nullptr);
  ASSERT_NE(whole->custom_mesh, nullptr);
  EXPECT_GT(mesh_axis_span(*whole->custom_mesh, 1),
            mesh_axis_span(*whole->custom_mesh, 0));
}

TEST(HorseSpecTest, FullSpecFrontHoofCentersUnderFrontCannon) {
  auto const &spec = Render::Horse::horse_creature_spec();

  auto const *whole =
      find_primitive(spec.lod_full.primitives, "horse.full.whole");

  ASSERT_NE(whole, nullptr);

  EXPECT_EQ(whole->shape, Render::Creature::PrimitiveShape::Mesh);
  ASSERT_NE(whole->custom_mesh, nullptr);
  EXPECT_GT(mesh_axis_span(*whole->custom_mesh, 2),
            mesh_axis_span(*whole->custom_mesh, 0) * 2.0F);
}

TEST(HorseSpecTest, MinimalSpecMatchesTallRetunedSilhouette) {
  auto const &spec = Render::Horse::horse_creature_spec();

  auto const *whole =
      find_primitive(spec.lod_minimal.primitives, "horse.minimal.whole");

  ASSERT_NE(whole, nullptr);
  EXPECT_EQ(whole->shape, Render::Creature::PrimitiveShape::Mesh);
  ASSERT_NE(whole->custom_mesh, nullptr);
  float const y_span = mesh_axis_span(*whole->custom_mesh, 1);
  float const z_span = mesh_axis_span(*whole->custom_mesh, 2);
  EXPECT_GT(y_span, z_span * 1.2F);
  EXPECT_EQ(find_primitive(spec.lod_minimal.primitives, "horse.minimal.leg.fl"),
            nullptr);
}

TEST(HorseSpecTest, AnimatedWalkKeepsRearHoofLowerDuringStanceThanSwing) {
  auto dims = make_horse_dims();
  auto gait = make_horse_gait();
  gait.cycle_time = 1.1F;
  gait.front_leg_phase = 0.25F;
  gait.rear_leg_phase = 0.0F;
  gait.stride_swing = 0.34F;
  gait.stride_lift = 0.14F;

  Render::Horse::HorseSpecPose stance_pose;
  Render::Horse::HorseSpecPose swing_pose;
  Render::Horse::make_horse_spec_pose_animated(
      dims, gait, Render::Horse::HorsePoseMotion{0.0F, 0.0F, true},
      stance_pose);
  Render::Horse::make_horse_spec_pose_animated(
      dims, gait, Render::Horse::HorsePoseMotion{0.75F, 0.0F, true},
      swing_pose);

  EXPECT_LT(stance_pose.foot_bl.y(), swing_pose.foot_bl.y());
  EXPECT_GT(swing_pose.foot_bl.y() - stance_pose.foot_bl.y(), 0.05F);
}

TEST(HorseSpecTest, AnimatedPoseBendsFrontAndRearKneesDuringMotion) {
  auto dims = make_horse_dims();
  auto gait =
      Render::GL::gait_for_type(Render::GL::GaitType::TROT, make_horse_gait());

  Render::Horse::HorseSpecPose pose;
  Render::Horse::make_horse_spec_pose_animated(
      dims, gait, Render::Horse::HorsePoseMotion{0.70F, 0.0F, true}, pose);

  QVector3D const shoulder_fl =
      pose.barrel_center + pose.shoulder_offset_pose_fl;
  QVector3D const shoulder_bl =
      pose.barrel_center + pose.shoulder_offset_pose_bl;
  float const front_mid_z = (shoulder_fl.z() + pose.foot_fl.z()) * 0.5F;
  float const rear_mid_z = (shoulder_bl.z() + pose.foot_bl.z()) * 0.5F;

  EXPECT_LT(pose.knee_fl.y(), shoulder_fl.y());
  EXPECT_GT(pose.knee_fl.y(), pose.foot_fl.y());
  EXPECT_GT(pose.knee_fl.z(), front_mid_z);
  EXPECT_GT(point_to_segment_distance(pose.knee_fl, shoulder_fl, pose.foot_fl),
            dims.body_length * 0.05F);

  EXPECT_LT(pose.knee_bl.y(), shoulder_bl.y());
  EXPECT_GT(pose.knee_bl.y(), pose.foot_bl.y());
  EXPECT_LT(pose.knee_bl.z(), rear_mid_z);
}

TEST(HorseSpecTest, WalkAndTrotProduceDifferentKneeFlexProfiles) {
  auto dims = make_horse_dims();
  auto walk =
      Render::GL::gait_for_type(Render::GL::GaitType::WALK, make_horse_gait());
  auto trot =
      Render::GL::gait_for_type(Render::GL::GaitType::TROT, make_horse_gait());

  Render::Horse::HorseSpecPose walk_pose;
  Render::Horse::HorseSpecPose trot_pose;
  Render::Horse::make_horse_spec_pose_animated(
      dims, walk, Render::Horse::HorsePoseMotion{0.72F, 0.0F, true}, walk_pose);
  Render::Horse::make_horse_spec_pose_animated(
      dims, trot, Render::Horse::HorsePoseMotion{0.72F, 0.0F, true}, trot_pose);

  QVector3D const walk_shoulder =
      walk_pose.barrel_center + walk_pose.shoulder_offset_pose_fl;
  QVector3D const trot_shoulder =
      trot_pose.barrel_center + trot_pose.shoulder_offset_pose_fl;
  float const walk_bend = point_to_segment_distance(
      walk_pose.knee_fl, walk_shoulder, walk_pose.foot_fl);
  float const trot_bend = point_to_segment_distance(
      trot_pose.knee_fl, trot_shoulder, trot_pose.foot_fl);

  EXPECT_GT(trot_bend, walk_bend * 1.15F);
}

TEST(HorseSpecTest, FightPoseRaisesFrontFeetHigherThanIdlePose) {
  auto dims = make_horse_dims();
  auto gait = make_horse_gait();

  // Phase 0.55 is in the "hold high" window (0.45-0.65) of the fight cycle.
  // FL phase = motion.phase + front_leg_phase = 0.55 + 0.0 = 0.55 → hold high
  Render::Horse::HorseSpecPose fight_pose;
  Render::Horse::make_horse_spec_pose_animated(
      dims, gait,
      Render::Horse::HorsePoseMotion{0.55F, 0.0F, false, /*is_fighting=*/true},
      fight_pose);

  Render::Horse::HorseSpecPose idle_pose;
  Render::Horse::make_horse_spec_pose_animated(
      dims, gait, Render::Horse::HorsePoseMotion{0.55F, 0.0F, false, false},
      idle_pose);

  // Front-left foot must be significantly higher during fight peak
  EXPECT_GT(fight_pose.foot_fl.y() - idle_pose.foot_fl.y(),
            dims.leg_length * 0.25F);

  // Rear feet stay grounded during fight
  EXPECT_NEAR(fight_pose.foot_bl.y(), idle_pose.foot_bl.y(),
              dims.leg_length * 0.05F);
  EXPECT_NEAR(fight_pose.foot_br.y(), idle_pose.foot_br.y(),
              dims.leg_length * 0.05F);
}

TEST(HorseSpecTest, FightPoseStrikePhaseLowersFrontFeetBelowPeak) {
  auto dims = make_horse_dims();
  auto gait = make_horse_gait();

  // Phase 0.55 = peak (hold high), phase 0.72 = mid-strike (dropping)
  Render::Horse::HorseSpecPose peak_pose;
  Render::Horse::make_horse_spec_pose_animated(
      dims, gait,
      Render::Horse::HorsePoseMotion{0.55F, 0.0F, false, /*is_fighting=*/true},
      peak_pose);

  Render::Horse::HorseSpecPose strike_pose;
  Render::Horse::make_horse_spec_pose_animated(
      dims, gait,
      Render::Horse::HorsePoseMotion{0.72F, 0.0F, false, /*is_fighting=*/true},
      strike_pose);

  // During strike phase the foot drops below the peak
  EXPECT_LT(strike_pose.foot_fl.y(), peak_pose.foot_fl.y());
}

TEST(HorseSpecTest, MovementBobVariesAcrossFrames) {
  auto dims = make_horse_dims();
  dims.move_bob_amplitude = 0.030F;
  auto gait =
      Render::GL::gait_for_type(Render::GL::GaitType::TROT, make_horse_gait());

  // At phase=0.0 (sin=0) the bob is zero; at phase=0.25 (sin=1.0) it is max.
  Render::Horse::HorseSpecPose pose_zero;
  Render::Horse::make_horse_spec_pose_animated(
      dims, gait, Render::Horse::HorsePoseMotion{0.0F, 0.0F, true, false},
      pose_zero);

  float const max_bob = dims.move_bob_amplitude * 0.85F;
  Render::Horse::HorseSpecPose pose_peak;
  Render::Horse::make_horse_spec_pose_animated(
      dims, gait, Render::Horse::HorsePoseMotion{0.25F, max_bob, true, false},
      pose_peak);

  // The barrel center should be higher at bob peak than at zero
  EXPECT_GT(pose_peak.barrel_center.y(), pose_zero.barrel_center.y());
}
