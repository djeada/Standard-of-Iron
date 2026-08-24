#include <gtest/gtest.h>

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"
#include "render/unit_render_cache.h"

namespace {

class UnitRenderCacheTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    Game::Systems::initialize_default_content(nations);

    world_view = Render::WorldView{};
  }

  Render::WorldView world_view;
};

TEST_F(UnitRenderCacheTest, UsesCanonicalBuildingRendererKeyWhenRenderableIdBlank) {
  Render::UnitRenderCache cache;

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>();
  ASSERT_NE(renderable, nullptr);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  entity.add_component<Engine::Core::BuildingComponent>();

  unit->spawn_type = Game::Units::SpawnType::Barracks;
  unit->nation_id = Game::Systems::NationID::Carthage;

  const auto& cached = cache.get_or_create(world_view, 1, &entity, 1);
  EXPECT_EQ(cached.renderer_key, "troops/carthage/barracks");
}

TEST_F(UnitRenderCacheTest, CanonicalizesPublicBuildingRendererKeyUsingBuildingNation) {
  Render::UnitRenderCache cache;

  Engine::Core::StandaloneEntity entity_scratch(2);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>();
  ASSERT_NE(renderable, nullptr);
  renderable->renderer_id = "barracks";
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  entity.add_component<Engine::Core::BuildingComponent>();

  unit->spawn_type = Game::Units::SpawnType::Barracks;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;

  const auto& cached = cache.get_or_create(world_view, 2, &entity, 1);
  EXPECT_EQ(cached.renderer_key, "troops/roman/barracks");
}

TEST_F(UnitRenderCacheTest, UsesTroopProfileRendererForBlankInfantryRendererId) {
  Render::UnitRenderCache cache;

  Engine::Core::StandaloneEntity entity_scratch(3);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>();
  ASSERT_NE(renderable, nullptr);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);

  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;

  const auto& cached = cache.get_or_create(world_view, 3, &entity, 1);
  EXPECT_EQ(cached.renderer_key, "troops/roman/swordsman");
}

TEST_F(UnitRenderCacheTest, ReplacesLegacySpawnTypeRendererIdWithProfileRenderer) {
  Render::UnitRenderCache cache;

  Engine::Core::StandaloneEntity entity_scratch(4);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>();
  ASSERT_NE(renderable, nullptr);
  renderable->renderer_id = "spearman";
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);

  unit->spawn_type = Game::Units::SpawnType::Spearman;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;

  const auto& cached = cache.get_or_create(world_view, 4, &entity, 1);
  EXPECT_EQ(cached.renderer_key, "troops/roman/spearman");
}

TEST_F(UnitRenderCacheTest, RefreshesRendererKeyAndInvalidatesHandleWhenInputsChange) {
  Render::UnitRenderCache cache;

  Engine::Core::StandaloneEntity entity_scratch(5);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>();
  ASSERT_NE(renderable, nullptr);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  entity.add_component<Engine::Core::BuildingComponent>();

  unit->spawn_type = Game::Units::SpawnType::Barracks;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;

  auto& first = cache.get_or_create(world_view, 5, &entity, 1);
  EXPECT_EQ(first.renderer_key, "troops/roman/barracks");

  first.renderer_handle = 17;
  first.has_renderer_handle = true;

  unit->nation_id = Game::Systems::NationID::Carthage;

  const auto& second = cache.get_or_create(world_view, 5, &entity, 2);
  EXPECT_EQ(second.renderer_key, "troops/carthage/barracks");
  EXPECT_FALSE(second.has_renderer_handle);
}

TEST_F(UnitRenderCacheTest, ModelMatrixFollowsTheCommanderPresentationPose) {
  Render::UnitRenderCache cache;

  Engine::Core::StandaloneEntity entity_scratch(41);
  Engine::Core::Entity& entity = entity_scratch.entity();
  entity.add_component<Engine::Core::RenderableComponent>();
  entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  auto* transform =
      entity.add_component<Engine::Core::TransformComponent>(0.0F, 0.0F, 4.0F);
  ASSERT_NE(transform, nullptr);

  auto* sample =
      entity.add_component<Engine::Core::CommanderPresentationSampleComponent>();
  ASSERT_NE(sample, nullptr);
  sample->valid = true;
  sample->snap = false;
  sample->tick_sequence = 7;
  sample->tick_seconds = 1.0F / 60.0F;
  sample->previous_position = {0.0F, 0.0F, 3.0F};
  sample->position = {0.0F, 0.0F, 4.0F};

  auto& cached = cache.get_or_create(world_view, 41, &entity, 1);
  ASSERT_NE(cached.presentation, nullptr);

  constexpr float k_quarter = (1.0F / 60.0F) * 0.25F;
  ASSERT_TRUE(Render::UnitRenderCache::update_model_matrix(cached, k_quarter));
  EXPECT_NEAR(cached.model_matrix.column(3).z(), 3.25F, 1.0e-4F)
      << "a quarter of a tick in, the body must sit a quarter of the way along";

  ASSERT_TRUE(Render::UnitRenderCache::update_model_matrix(cached, k_quarter * 3.0F));
  EXPECT_NEAR(cached.model_matrix.column(3).z(), 4.0F, 1.0e-4F)
      << "a whole tick in, the body must sit on the authoritative sample";
}

TEST_F(UnitRenderCacheTest, ASnappedPresentationSampleIsNotInterpolated) {
  Render::UnitRenderCache cache;

  Engine::Core::StandaloneEntity entity_scratch(42);
  Engine::Core::Entity& entity = entity_scratch.entity();
  entity.add_component<Engine::Core::RenderableComponent>();
  entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  entity.add_component<Engine::Core::TransformComponent>(0.0F, 0.0F, 40.0F);

  auto* sample =
      entity.add_component<Engine::Core::CommanderPresentationSampleComponent>();
  ASSERT_NE(sample, nullptr);
  sample->valid = true;
  sample->snap = true;
  sample->tick_sequence = 3;
  sample->tick_seconds = 1.0F / 60.0F;
  sample->previous_position = {0.0F, 0.0F, 0.0F};
  sample->position = {0.0F, 0.0F, 40.0F};

  auto& cached = cache.get_or_create(world_view, 42, &entity, 1);
  ASSERT_TRUE(
      Render::UnitRenderCache::update_model_matrix(cached, (1.0F / 60.0F) * 0.25F));
  EXPECT_NEAR(cached.model_matrix.column(3).z(), 40.0F, 1.0e-4F)
      << "a teleport must not be smeared across the screen";
}

} // namespace
