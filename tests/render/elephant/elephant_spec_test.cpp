// Stage 16.5 — elephant rigged-path body submit smoke tests.
//
// The legacy walker entry point (`submit_elephant_lod`) was removed
// when the imperative LOD path was retired; the rig now exclusively
// calls the rigged wrappers. With a non-Renderer submitter we exercise
// the software fallback inside `submit_elephant_*_rigged`, which routes
// through `submit_creature(spec, lod)` and emits one draw per static
// PartGraph primitive (5/41 for Minimal/Full).

#include "render/creature/spec.h"
#include "render/elephant/elephant_renderer_base.h"
#include "render/elephant/elephant_spec.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
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

Render::GL::ElephantDimensions make_dims() {
  Render::GL::ElephantDimensions d{};
  d.body_length = 3.2F;
  d.body_width = 1.1F;
  d.body_height = 1.6F;
  d.barrel_center_y = 2.0F;
  d.neck_length = 0.9F;
  d.neck_width = 0.42F;
  d.head_length = 1.1F;
  d.head_width = 0.75F;
  d.head_height = 0.78F;
  d.trunk_length = 1.5F;
  d.trunk_base_radius = 0.22F;
  d.trunk_tip_radius = 0.06F;
  d.ear_width = 1.0F;
  d.ear_height = 1.1F;
  d.ear_thickness = 0.06F;
  d.leg_length = 1.6F;
  d.leg_radius = 0.22F;
  d.foot_radius = 0.28F;
  d.tail_length = 0.8F;
  d.tusk_length = 0.55F;
  d.tusk_radius = 0.06F;
  return d;
}

Render::GL::ElephantVariant make_variant() {
  Render::GL::ElephantVariant v{};
  v.skin_color = QVector3D(0.45F, 0.40F, 0.38F);
  return v;
}

Render::GL::ElephantGait make_gait() {
  Render::GL::ElephantGait g{};
  g.cycle_time = 1.2F;
  g.front_leg_phase = 0.0F;
  g.rear_leg_phase = 0.5F;
  g.stride_swing = 0.30F;
  g.stride_lift = 0.10F;
  return g;
}

} // namespace

TEST(ElephantSpecTest, CreatureSpecHasTwoLods) {
  auto const &spec = Render::Elephant::elephant_creature_spec();
  EXPECT_EQ(spec.lod_minimal.primitives.size(), 5U);
  EXPECT_EQ(spec.lod_full.primitives.size(), 41U);
}

TEST(ElephantSpecTest, PoseKeepsHeavyBodyAndPillarLegReadability) {
  auto const dims = make_dims();
  Render::Elephant::ElephantSpecPose pose{};
  Render::Elephant::make_elephant_spec_pose(dims, 0.0F, pose);

  EXPECT_GT(pose.body_ellipsoid_x, dims.body_width);
  EXPECT_GT(pose.body_ellipsoid_z, dims.body_length);
  EXPECT_LT(pose.body_ellipsoid_x, dims.body_width * 1.3F);
  EXPECT_GE(pose.leg_radius, dims.leg_radius * 0.69F);
  EXPECT_LE(pose.shoulder_offset_fl.y(), -dims.body_height * 0.25F);
  EXPECT_LT(pose.foot_fl.y(), pose.barrel_center.y());
  EXPECT_LT(pose.foot_fl.y(), pose.barrel_center.y() - dims.leg_length * 0.6F);
}

TEST(ElephantSpecTest, AnimatedPoseKeepsForwardTrunkAndLighterHeadRead) {
  auto const dims = make_dims();
  auto const gait = make_gait();
  Render::Elephant::ElephantSpecPose pose{};
  Render::Elephant::make_elephant_spec_pose_animated(
      dims, gait, Render::Elephant::ElephantPoseMotion{}, pose);

  EXPECT_LT(pose.head_half.x(), dims.head_width * 0.45F);
  EXPECT_LT(pose.head_half.y(), dims.head_height * 0.41F);
  EXPECT_GT(pose.trunk_end.z(),
            pose.head_center.z() + dims.trunk_length * 0.35F);
  EXPECT_LT(pose.trunk_end.y(),
            pose.head_center.y() - dims.trunk_length * 0.45F);
  EXPECT_LT(pose.pose_leg_radius, dims.leg_radius * 0.9F);
}
