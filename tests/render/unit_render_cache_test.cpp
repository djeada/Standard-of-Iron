#include <gtest/gtest.h>

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/systems/nation_registry.h"
#include "render/unit_render_cache.h"

namespace {

class UnitRenderCacheTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    nations.initialize_defaults();
  }
};

TEST_F(UnitRenderCacheTest, UsesCanonicalBuildingRendererKeyWhenRenderableIdBlank) {
  Render::UnitRenderCache cache;

  Engine::Core::Entity entity(1);
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
  ASSERT_NE(renderable, nullptr);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  entity.add_component<Engine::Core::BuildingComponent>();

  unit->spawn_type = Game::Units::SpawnType::Barracks;
  unit->nation_id = Game::Systems::NationID::Carthage;

  const auto& cached = cache.get_or_create(1, &entity, 1);
  EXPECT_EQ(cached.renderer_key, "troops/carthage/barracks");
}

TEST_F(UnitRenderCacheTest, CanonicalizesPublicBuildingRendererKeyUsingBuildingNation) {
  Render::UnitRenderCache cache;

  Engine::Core::Entity entity(2);
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
  ASSERT_NE(renderable, nullptr);
  renderable->renderer_id = "barracks";
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  entity.add_component<Engine::Core::BuildingComponent>();

  unit->spawn_type = Game::Units::SpawnType::Barracks;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;

  const auto& cached = cache.get_or_create(2, &entity, 1);
  EXPECT_EQ(cached.renderer_key, "troops/roman/barracks");
}

TEST_F(UnitRenderCacheTest, UsesTroopProfileRendererForBlankInfantryRendererId) {
  Render::UnitRenderCache cache;

  Engine::Core::Entity entity(3);
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
  ASSERT_NE(renderable, nullptr);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);

  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;

  const auto& cached = cache.get_or_create(3, &entity, 1);
  EXPECT_EQ(cached.renderer_key, "troops/roman/swordsman");
}

TEST_F(UnitRenderCacheTest, ReplacesLegacySpawnTypeRendererIdWithProfileRenderer) {
  Render::UnitRenderCache cache;

  Engine::Core::Entity entity(4);
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
  ASSERT_NE(renderable, nullptr);
  renderable->renderer_id = "spearman";
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(unit, nullptr);

  unit->spawn_type = Game::Units::SpawnType::Spearman;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;

  const auto& cached = cache.get_or_create(4, &entity, 1);
  EXPECT_EQ(cached.renderer_key, "troops/roman/spearman");
}

TEST_F(UnitRenderCacheTest, RefreshesRendererKeyAndInvalidatesHandleWhenInputsChange) {
  Render::UnitRenderCache cache;

  Engine::Core::Entity entity(5);
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
  ASSERT_NE(renderable, nullptr);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  entity.add_component<Engine::Core::BuildingComponent>();

  unit->spawn_type = Game::Units::SpawnType::Barracks;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;

  auto& first = cache.get_or_create(5, &entity, 1);
  EXPECT_EQ(first.renderer_key, "troops/roman/barracks");

  first.renderer_handle = 17;
  first.has_renderer_handle = true;

  unit->nation_id = Game::Systems::NationID::Carthage;

  const auto& second = cache.get_or_create(5, &entity, 2);
  EXPECT_EQ(second.renderer_key, "troops/carthage/barracks");
  EXPECT_FALSE(second.has_renderer_handle);
}

} // namespace
