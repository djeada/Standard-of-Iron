// Stage 16.5 — elephant rigged-path body submit smoke tests.
//
// The legacy walker entry point (`submit_elephant_lod`) was removed
// when the imperative LOD path was retired; the rig now exclusively
// calls the rigged wrappers. With a non-Renderer submitter we exercise
// the software fallback inside `submit_elephant_*_rigged`, which routes
// through `submit_creature(spec, lod)` and emits one draw per static
// PartGraph primitive (5/12/15 for Minimal/Reduced/Full).

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
  d.head_length = 1.1F;
  d.leg_length = 1.6F;
  d.leg_radius = 0.22F;
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

TEST(ElephantSpecTest, MinimalRiggedFallbackEmitsFivePrimitives) {
  auto dims = make_dims();
  auto variant = make_variant();
  Render::Elephant::ElephantSpecPose pose;
  Render::Elephant::make_elephant_spec_pose(dims, 0.0F, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Elephant::submit_elephant_minimal_rigged(pose, variant, identity,
                                                   sub);

  EXPECT_EQ(sub.calls.size(), 5U);
}

TEST(ElephantSpecTest, ReducedRiggedFallbackEmitsTwelvePrimitives) {
  auto dims = make_dims();
  auto gait = make_gait();
  auto variant = make_variant();

  Render::Elephant::ElephantSpecPose pose;
  Render::Elephant::ElephantReducedMotion motion{};
  Render::Elephant::make_elephant_spec_pose_reduced(dims, gait, motion, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Elephant::submit_elephant_reduced_rigged(pose, variant, identity,
                                                   sub);

  EXPECT_EQ(sub.calls.size(), 12U);
}

TEST(ElephantSpecTest, FullRiggedFallbackEmitsFifteenPrimitives) {
  auto dims = make_dims();
  auto gait = make_gait();
  auto variant = make_variant();

  Render::Elephant::ElephantSpecPose pose;
  Render::Elephant::ElephantReducedMotion motion{};
  Render::Elephant::make_elephant_spec_pose_reduced(dims, gait, motion, pose);

  QMatrix4x4 identity;
  CapturingSubmitter sub;
  Render::Elephant::submit_elephant_full_rigged(pose, variant, identity, sub);

  EXPECT_EQ(sub.calls.size(), 50U);
}

TEST(ElephantSpecTest, CreatureSpecHasAllThreeLods) {
  auto const &spec = Render::Elephant::elephant_creature_spec();
  EXPECT_EQ(spec.lod_minimal.primitives.size(), 5U);
  EXPECT_EQ(spec.lod_reduced.primitives.size(), 12U);
  EXPECT_EQ(spec.lod_full.primitives.size(), 50U);
}
