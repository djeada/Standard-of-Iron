#include "game/core/component.h"
#include "game/core/entity.h"
#include "render/entity/building_render_common.h"
#include "render/entity/registry.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace {

struct RecordedMesh {
  Render::GL::Mesh *mesh{nullptr};
  QMatrix4x4 model;
  QVector3D color{1.0F, 1.0F, 1.0F};
};

class RecordingSubmitter final : public Render::GL::ISubmitter {
public:
  std::vector<RecordedMesh> meshes;

  void mesh(Render::GL::Mesh *mesh, const QMatrix4x4 &model,
            const QVector3D &color, Render::GL::Texture *, float,
            int) override {
    meshes.push_back({mesh, model, color});
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

auto fake_mesh(int id) -> Render::GL::Mesh * {
  return reinterpret_cast<Render::GL::Mesh *>(static_cast<intptr_t>(id));
}

TEST(BuildingRenderCommon, SelectsNationVariantRendererKey) {
  using namespace Render::GL;

  EXPECT_EQ(building_renderer_key("roman", "home"), "troops/roman/home");
  EXPECT_EQ(
      building_renderer_key(Game::Systems::NationID::Carthage, "barracks"),
      "troops/carthage/barracks");
  EXPECT_EQ(canonicalize_building_renderer_key("barracks_roman"),
            "troops/roman/barracks");
  EXPECT_EQ(canonicalize_building_renderer_key("barracks_carthage"),
            "troops/carthage/barracks");
  EXPECT_EQ(resolve_building_renderer_key(
                "", "defense_tower", Game::Systems::NationID::RomanRepublic),
            "troops/roman/defense_tower");
  EXPECT_EQ(resolve_building_renderer_key("barracks", "barracks",
                                          Game::Systems::NationID::Carthage),
            "troops/carthage/barracks");
  EXPECT_EQ(canonicalize_building_renderer_key("home"), "home");
  EXPECT_EQ(select_nation_variant_renderer_key(
                "roman", "carthage", Game::Systems::NationID::RomanRepublic),
            "roman");
  EXPECT_EQ(select_nation_variant_renderer_key(
                "roman", "carthage", Game::Systems::NationID::Carthage),
            "carthage");
}

TEST(BuildingRenderCommon, ResolvesBuildingStateFromUnitHealth) {
  using namespace Render::GL;

  Engine::Core::Entity entity(1);
  auto *unit =
      entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);

  DrawContext ctx;
  ctx.entity = &entity;

  unit->health = 85;
  EXPECT_EQ(resolve_building_state(ctx), BuildingState::Normal);

  unit->health = 50;
  EXPECT_EQ(resolve_building_state(ctx), BuildingState::Damaged);

  unit->health = 10;
  EXPECT_EQ(resolve_building_state(ctx), BuildingState::Destroyed);
}

TEST(BuildingRenderCommon, RegisteredVariantDispatcherRoutesByNation) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  int roman_calls = 0;
  int carthage_calls = 0;
  registry.register_renderer(
      "troops/roman/test_building",
      [&roman_calls](const DrawContext &, ISubmitter &) { ++roman_calls; });
  registry.register_renderer(
      "troops/carthage/test_building",
      [&carthage_calls](const DrawContext &, ISubmitter &) {
        ++carthage_calls;
      });
  register_nation_variant_renderer(registry, "test_building",
                                   "troops/roman/test_building",
                                   "troops/carthage/test_building");

  const auto dispatcher = registry.get("test_building");
  ASSERT_TRUE(static_cast<bool>(dispatcher));

  Engine::Core::Entity entity(2);
  auto *unit =
      entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);

  DrawContext ctx;
  ctx.entity = &entity;
  RecordingSubmitter submitter;

  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  dispatcher(ctx, submitter);
  EXPECT_EQ(roman_calls, 1);
  EXPECT_EQ(carthage_calls, 0);

  unit->nation_id = Game::Systems::NationID::Carthage;
  dispatcher(ctx, submitter);
  EXPECT_EQ(roman_calls, 1);
  EXPECT_EQ(carthage_calls, 1);
}

TEST(BuildingRenderCommon, BuildingInstanceSelectsFullLodWhenNearby) {
  using namespace Render::GL;
  reset_building_instance_cache_for_tests();

  RenderArchetypeBuilder builder("building_lod_test");
  builder.set_max_distance(60.0F);
  builder.add_mesh(fake_mesh(1), QMatrix4x4{}, QVector3D(1.0F, 0.0F, 0.0F));
  builder.use_lod(RenderArchetypeLod::Minimal);
  builder.add_mesh(fake_mesh(2), QMatrix4x4{}, QVector3D(0.0F, 1.0F, 0.0F));
  RenderArchetype archetype = std::move(builder).build();

  DrawContext ctx;
  ctx.distance_sq = 10.0F * 10.0F;
  RecordingSubmitter submitter;
  submit_building_instance(submitter, ctx, archetype);

  ASSERT_EQ(submitter.meshes.size(), 1u);
  EXPECT_EQ(submitter.meshes.front().mesh, fake_mesh(1));
}

TEST(BuildingRenderCommon, BuildingInstanceSelectsMinimalLodWhenFar) {
  using namespace Render::GL;
  reset_building_instance_cache_for_tests();

  RenderArchetypeBuilder builder("building_lod_test_far");
  builder.set_max_distance(60.0F);
  builder.add_mesh(fake_mesh(1), QMatrix4x4{}, QVector3D(1.0F, 0.0F, 0.0F));
  builder.use_lod(RenderArchetypeLod::Minimal);
  builder.add_mesh(fake_mesh(2), QMatrix4x4{}, QVector3D(0.0F, 1.0F, 0.0F));
  RenderArchetype archetype = std::move(builder).build();

  DrawContext ctx;
  ctx.distance_sq = 80.0F * 80.0F;
  RecordingSubmitter submitter;
  submit_building_instance(submitter, ctx, archetype);

  ASSERT_EQ(submitter.meshes.size(), 1u);
  EXPECT_EQ(submitter.meshes.front().mesh, fake_mesh(2));
}

TEST(BuildingRenderCommon, RegisterBuildingRendererUsesCanonicalKeyOnly) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  register_building_renderer(registry, "roman", "barracks",
                             [](const DrawContext &, ISubmitter &) {});

  EXPECT_TRUE(static_cast<bool>(registry.get("troops/roman/barracks")));
  EXPECT_FALSE(static_cast<bool>(registry.get("barracks_roman")));
}

TEST(BuildingRenderCommon, BuildingInstanceCacheReusesUnchangedEntityData) {
  using namespace Render::GL;
  reset_building_instance_cache_for_tests();

  RenderArchetypeBuilder builder("building_cache_same");
  builder.set_max_distance(60.0F);
  builder.add_mesh(fake_mesh(1), QMatrix4x4{}, QVector3D(1.0F, 0.0F, 0.0F));
  builder.use_lod(RenderArchetypeLod::Minimal);
  builder.add_mesh(fake_mesh(2), QMatrix4x4{}, QVector3D(0.0F, 1.0F, 0.0F));
  RenderArchetype archetype = std::move(builder).build();

  Engine::Core::Entity entity(77);
  DrawContext ctx;
  ctx.entity = &entity;
  ctx.distance_sq = 20.0F * 20.0F;
  RecordingSubmitter submitter;

  submit_building_instance(submitter, ctx, archetype);
  const auto stats_after_first = get_building_instance_cache_stats();
  submit_building_instance(submitter, ctx, archetype);
  const auto stats_after_second = get_building_instance_cache_stats();

  EXPECT_EQ(stats_after_first.misses, 1U);
  EXPECT_EQ(stats_after_first.rebuilds, 1U);
  EXPECT_EQ(stats_after_second.hits, 1U);
  EXPECT_EQ(stats_after_second.rebuilds, 1U);
}

TEST(BuildingRenderCommon, BuildingInstanceCacheRebuildsOnLodChange) {
  using namespace Render::GL;
  reset_building_instance_cache_for_tests();

  RenderArchetypeBuilder builder("building_cache_lod_change");
  builder.set_max_distance(60.0F);
  builder.add_mesh(fake_mesh(1), QMatrix4x4{}, QVector3D(1.0F, 0.0F, 0.0F));
  builder.use_lod(RenderArchetypeLod::Minimal);
  builder.add_mesh(fake_mesh(2), QMatrix4x4{}, QVector3D(0.0F, 1.0F, 0.0F));
  RenderArchetype archetype = std::move(builder).build();

  Engine::Core::Entity entity(78);
  DrawContext ctx;
  ctx.entity = &entity;
  RecordingSubmitter submitter;

  ctx.distance_sq = 10.0F * 10.0F;
  submit_building_instance(submitter, ctx, archetype);
  const auto stats_after_near = get_building_instance_cache_stats();

  ctx.distance_sq = 90.0F * 90.0F;
  submit_building_instance(submitter, ctx, archetype);
  const auto stats_after_far = get_building_instance_cache_stats();

  ASSERT_GE(submitter.meshes.size(), 2U);
  EXPECT_EQ(submitter.meshes[0].mesh, fake_mesh(1));
  EXPECT_EQ(submitter.meshes[1].mesh, fake_mesh(2));
  EXPECT_EQ(stats_after_near.rebuilds, 1U);
  EXPECT_EQ(stats_after_far.rebuilds, 2U);
  EXPECT_EQ(stats_after_far.hits, 1U);
}

} // namespace
