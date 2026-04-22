// Phase A regression — horse prepare module.
//
// Verifies make_horse_prepared_row stamps the correct kind/pass/lod and that
// Shadow-tagged horse rows are filtered by submit().

#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "render/creature/pipeline/creature_render_state.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/horse/dimensions.h"
#include "render/horse/horse_renderer_base.h"
#include "render/horse/horse_spec.h"
#include "render/horse/prepare.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <gtest/gtest.h>
#include <vector>

namespace {

class NullSubmitter : public Render::GL::ISubmitter {
public:
  int rigged_calls{0};
  void mesh(Render::GL::Mesh *, const QMatrix4x4 &, const QVector3D &,
            Render::GL::Texture *, float, int) override {}
  void rigged(const Render::GL::RiggedCreatureCmd &) override {
    ++rigged_calls;
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

struct ScopedFlatTerrain {
  explicit ScopedFlatTerrain(float height) {
    auto &terrain = Game::Map::TerrainService::instance();
    std::vector<float> heights(9, height);
    std::vector<Game::Map::TerrainType> terrain_types(
        9, Game::Map::TerrainType::Flat);
    terrain.restore_from_serialized(3, 3, 1.0F, heights, terrain_types, {}, {},
                                    {}, Game::Map::BiomeSettings{});
  }

  ~ScopedFlatTerrain() { Game::Map::TerrainService::instance().clear(); }
};

TEST(HorsePrepare, MakePreparedHorseRowStampsKindAndPass) {
  Render::Creature::Pipeline::UnitVisualSpec spec{};
  spec.kind = Render::Creature::Pipeline::CreatureKind::Horse;

  Render::Horse::HorseSpecPose pose{};
  Render::GL::HorseVariant variant{};
  QMatrix4x4 world;
  const auto row = Render::Creature::Pipeline::make_prepared_horse_row(
      spec, pose, variant, world, /*seed*/ 11,
      Render::Creature::CreatureLOD::Reduced,
      /*entity_id*/ 0, Render::Creature::Pipeline::RenderPassIntent::Shadow);

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

TEST(HorsePrepare, MinimalPreparationSnapsHorseBodyToTerrainHeight) {
  ScopedFlatTerrain terrain(1.9F);

  Render::GL::HorseRendererBase owner;
  Render::GL::DrawContext ctx{};
  ctx.model.translate(-0.3F, 6.0F, 0.45F);

  Render::GL::HorseProfile profile = Render::GL::make_horse_profile(
      17U, QVector3D(0.4F, 0.3F, 0.2F), QVector3D(0.6F, 0.1F, 0.1F));

  Render::Horse::HorsePreparation prep;
  Render::Horse::prepare_horse_minimal(owner, ctx, profile, nullptr, prep);

  auto const rows = prep.bodies.rows();
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_NEAR(rows[0].world_from_unit.map(QVector3D(0.0F, 0.0F, 0.0F)).y(),
              1.9F, 0.0001F);
}

} // namespace
