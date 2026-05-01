#include "game/core/component.h"
#include "game/core/entity.h"
#include "render/unit_render_cache.h"

#include <gtest/gtest.h>

namespace {

TEST(UnitRenderCache, UsesCanonicalBuildingRendererKeyWhenRenderableIdBlank) {
  Render::UnitRenderCache cache;

  Engine::Core::Entity entity(1);
  auto *renderable =
      entity.add_component<Engine::Core::RenderableComponent>("", "");
  ASSERT_NE(renderable, nullptr);
  auto *unit =
      entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  entity.add_component<Engine::Core::BuildingComponent>();

  unit->spawn_type = Game::Units::SpawnType::Barracks;
  unit->nation_id = Game::Systems::NationID::Carthage;

  const auto &cached = cache.get_or_create(1, &entity, 1);
  EXPECT_EQ(cached.renderer_key, "troops/carthage/barracks");
}

TEST(UnitRenderCache,
     CanonicalizesPublicBuildingRendererKeyUsingBuildingNation) {
  Render::UnitRenderCache cache;

  Engine::Core::Entity entity(2);
  auto *renderable =
      entity.add_component<Engine::Core::RenderableComponent>("", "");
  ASSERT_NE(renderable, nullptr);
  renderable->renderer_id = "barracks";
  auto *unit =
      entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  entity.add_component<Engine::Core::BuildingComponent>();

  unit->spawn_type = Game::Units::SpawnType::Barracks;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;

  const auto &cached = cache.get_or_create(2, &entity, 1);
  EXPECT_EQ(cached.renderer_key, "troops/roman/barracks");
}

} // namespace
