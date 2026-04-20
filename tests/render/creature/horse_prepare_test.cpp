// Phase A regression — horse prepare module.
//
// Verifies make_horse_prepared_row stamps the correct kind/pass/lod and that
// Shadow-tagged horse rows are filtered by submit().

#include "render/creature/pipeline/creature_render_state.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/horse/horse_spec.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <gtest/gtest.h>

namespace {

class NullSubmitter : public Render::GL::ISubmitter {
public:
  int rigged_calls{0};
  void mesh(Render::GL::Mesh *, const QMatrix4x4 &, const QVector3D &,
            Render::GL::Texture *, float, int) override {}
  void rigged(const Render::GL::RiggedCreatureCmd &) override { ++rigged_calls; }
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

TEST(HorsePrepare, MakePreparedHorseRowStampsKindAndPass) {
  Render::Creature::Pipeline::UnitVisualSpec spec{};
  spec.kind = Render::Creature::Pipeline::CreatureKind::Horse;

  Render::Horse::HorseSpecPose pose{};
  Render::GL::HorseVariant variant{};
  QMatrix4x4 world;
  const auto row = Render::Creature::Pipeline::make_prepared_horse_row(
      spec, pose, variant, world, /*seed*/ 11, Render::Creature::CreatureLOD::Reduced,
      /*entity_id*/ 0,
      Render::Creature::Pipeline::RenderPassIntent::Shadow);

  EXPECT_EQ(row.spec.kind, Render::Creature::Pipeline::CreatureKind::Horse);
  EXPECT_EQ(row.lod, Render::Creature::CreatureLOD::Reduced);
  EXPECT_EQ(row.pass, Render::Creature::Pipeline::RenderPassIntent::Shadow);
  EXPECT_EQ(row.seed, 11u);
}

TEST(HorsePrepare, ShadowHorseRowProducesNoDraw) {
  Render::Creature::Pipeline::PreparedCreatureRenderRow row{};
  row.spec.kind = Render::Creature::Pipeline::CreatureKind::Horse;
  row.lod = Render::Creature::CreatureLOD::Full;
  row.pass = Render::Creature::Pipeline::RenderPassIntent::Shadow;

  NullSubmitter sink;
  Render::Creature::Pipeline::PreparedCreatureSubmitBatch batch;
  batch.add(row);
  (void)batch.submit(sink);

  EXPECT_EQ(sink.rigged_calls, 0);
}

TEST(HorsePrepare, MainHorseRowProducesEntitySubmission) {
  Render::Creature::Pipeline::PreparedCreatureRenderRow row{};
  row.spec.kind = Render::Creature::Pipeline::CreatureKind::Horse;
  row.lod = Render::Creature::CreatureLOD::Full;
  row.pass = Render::Creature::Pipeline::RenderPassIntent::Main;

  NullSubmitter sink;
  Render::Creature::Pipeline::PreparedCreatureSubmitBatch batch;
  batch.add(row);
  const auto stats = batch.submit(sink);

  EXPECT_EQ(stats.entities_submitted, 1u);
}

} // namespace
