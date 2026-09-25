#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

#include "game/core/component_core.h"
#include "game/core/entity.h"
#include "render/entity/building_render_common.h"
#include "render/entity/registry.h"
#include "render/entity/wall_renderer_common.h"
#include "render/submitter.h"

namespace {

struct RecordedMesh {
  Render::GL::Mesh* mesh{nullptr};
  QMatrix4x4 model;
  QVector3D color{1.0F, 1.0F, 1.0F};
  int material_id{0};
};

class RecordingSubmitter final : public Render::GL::ISubmitter {
public:
  std::vector<RecordedMesh> meshes;

  void mesh(Render::GL::Mesh* mesh,
            const QMatrix4x4& model,
            const QVector3D& color,
            Render::GL::Texture*,
            float,
            int material_id) override {
    meshes.push_back({mesh, model, color, material_id});
  }

  void cylinder(
      const QVector3D&, const QVector3D&, float, const QVector3D&, float) override {}
  void ground_marker(const Render::GL::GroundMarkerCmd&) override {}
  void grid(const QMatrix4x4&, const QVector3D&, float, float, float) override {}
  void selection_smoke(const QMatrix4x4&, const QVector3D&, float) override {}
  void healing_beam(const QVector3D&,
                    const QVector3D&,
                    const QVector3D&,
                    float,
                    float,
                    float,
                    float) override {}
  void healer_aura(const QVector3D&, const QVector3D&, float, float, float) override {}
  void combat_dust(const QVector3D&, const QVector3D&, float, float, float) override {}
  void stone_impact(const QVector3D&, const QVector3D&, float, float, float) override {}
  void mode_indicator(const QMatrix4x4&, int, const QVector3D&, float) override {}
};

auto fake_mesh(int id) -> Render::GL::Mesh* {
  return reinterpret_cast<Render::GL::Mesh*>(static_cast<intptr_t>(id));
}

TEST(BuildingRenderCommon, SelectsNationVariantRendererKey) {
  using namespace Render::GL;

  EXPECT_EQ(building_renderer_key("roman", "home"), "troops/roman/home");
  EXPECT_EQ(building_renderer_key(Game::Systems::NationID::Carthage, "barracks"),
            "troops/carthage/barracks");
  EXPECT_EQ(canonicalize_building_renderer_key("barracks_roman"),
            "troops/roman/barracks");
  EXPECT_EQ(canonicalize_building_renderer_key("barracks_carthage"),
            "troops/carthage/barracks");
  EXPECT_EQ(resolve_building_renderer_key(
                "", "defense_tower", Game::Systems::NationID::RomanRepublic),
            "troops/roman/defense_tower");
  EXPECT_EQ(resolve_building_renderer_key(
                "barracks", "barracks", Game::Systems::NationID::Carthage),
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

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
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

TEST(BuildingRenderCommon, DamageMaterialTierPreservesSurfaceMaterial) {
  using namespace Render::GL;

  for (const int damage_tier : {10, 20}) {
    for (const int surface : {0, 1, 2, 3, 4}) {
      const int resolved = damage_material_id(surface, damage_tier);
      EXPECT_EQ(resolved % 10, surface);
      EXPECT_EQ(resolved / 10, damage_tier / 10);
    }
  }
}

TEST(BuildingRenderCommon, WallArchetypeSetDiffersPerState) {
  using namespace Render::GL;

  const WallArchetypeSet set =
      build_wall_archetype_set("test_wall", WallPalette{}, WallGeometry{});

  for (const auto& variant : set.variants) {
    const std::size_t normal =
        variant.for_state(BuildingState::Normal).lods[0].draws.size();
    const std::size_t damaged =
        variant.for_state(BuildingState::Damaged).lods[0].draws.size();
    const std::size_t destroyed =
        variant.for_state(BuildingState::Destroyed).lods[0].draws.size();

    EXPECT_GT(normal, 0U);
    EXPECT_NE(normal, damaged);
    EXPECT_NE(damaged, destroyed);
  }
}

TEST(BuildingRenderCommon, RegisteredVariantDispatcherRoutesByNation) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  int roman_calls = 0;
  int carthage_calls = 0;
  registry.register_renderer(
      "troops/roman/test_building",
      [&roman_calls](const DrawContext&, ISubmitter&) { ++roman_calls; });
  registry.register_renderer(
      "troops/carthage/test_building",
      [&carthage_calls](const DrawContext&, ISubmitter&) { ++carthage_calls; });
  register_nation_variant_renderer(registry,
                                   "test_building",
                                   "troops/roman/test_building",
                                   "troops/carthage/test_building");

  const auto dispatcher = registry.get("test_building");
  ASSERT_TRUE(static_cast<bool>(dispatcher));

  Engine::Core::StandaloneEntity entity_scratch(2);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
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

TEST(BuildingRenderCommon, BuildingInstanceCarriesEntityAndDamageState) {
  using namespace Render::GL;

  RenderArchetypeBuilder builder("building_instance_forwarding");
  builder.add_mesh(fake_mesh(1), QMatrix4x4{}, QVector3D(1.0F, 0.0F, 0.0F));
  builder.add_mesh(fake_mesh(2), QMatrix4x4{}, QVector3D(0.0F, 1.0F, 0.0F));
  RenderArchetype archetype = std::move(builder).build();

  Engine::Core::StandaloneEntity entity_scratch(77);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  unit->health = 10;

  DrawContext ctx;
  ctx.entity = &entity;
  ctx.model.translate(3.0F, 0.0F, 0.0F);
  RecordingSubmitter submitter;
  submit_building_instance(submitter, ctx, archetype);

  ASSERT_EQ(submitter.meshes.size(), 2U);
  EXPECT_EQ(submitter.meshes[0].mesh, fake_mesh(1));
  EXPECT_EQ(submitter.meshes[1].mesh, fake_mesh(2));
  EXPECT_FLOAT_EQ(submitter.meshes[0].model(0, 3), 3.0F);
  EXPECT_EQ(submitter.meshes[0].material_id, 20);
}

TEST(BuildingRenderCommon, PreviewBuildingInstanceHasNoStaticIdentity) {
  using namespace Render::GL;

  class InstanceRecorder final : public ForwardingSubmitter {
  public:
    using ForwardingSubmitter::ForwardingSubmitter;
    std::vector<RenderInstance> instances;
    void render_instance(const RenderInstance& instance) override {
      instances.push_back(instance);
    }
  };

  RenderArchetypeBuilder builder("building_preview_identity");
  builder.add_mesh(fake_mesh(1), QMatrix4x4{}, QVector3D(1.0F, 0.0F, 0.0F));
  RenderArchetype archetype = std::move(builder).build();

  RecordingSubmitter sink;
  InstanceRecorder recorder(sink);
  DrawContext preview_ctx;
  submit_building_instance(recorder, preview_ctx, archetype);

  Engine::Core::StandaloneEntity entity_scratch(91);
  DrawContext entity_ctx;
  entity_ctx.entity = &entity_scratch.entity();
  submit_building_instance(recorder, entity_ctx, archetype);

  ASSERT_EQ(recorder.instances.size(), 2U);
  EXPECT_EQ(recorder.instances[0].static_id, 0U);
  EXPECT_EQ(recorder.instances[1].static_id, 91U);
}

TEST(BuildingRenderCommon, RegisterBuildingRendererUsesCanonicalKeyOnly) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  register_building_renderer(
      registry, "roman", "barracks", [](const DrawContext&, ISubmitter&) {});

  EXPECT_TRUE(static_cast<bool>(registry.get("troops/roman/barracks")));
  EXPECT_FALSE(static_cast<bool>(registry.get("barracks_roman")));
}

} // namespace
