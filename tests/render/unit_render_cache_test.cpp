#include <algorithm>
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

  constexpr float k_tick = 1.0F / 60.0F;
  constexpr float k_quarter = k_tick * 0.25F;

  ASSERT_TRUE(Render::UnitRenderCache::update_model_matrix(cached, k_tick));
  EXPECT_NEAR(cached.model_matrix.column(3).z(), 4.0F, 1.0e-4F)
      << "a frame that lands on a tick must show the authoritative sample";

  ASSERT_TRUE(Render::UnitRenderCache::update_model_matrix(cached, k_quarter));
  EXPECT_NEAR(cached.model_matrix.column(3).z(), 4.25F, 1.0e-4F)
      << "between ticks the body must carry on rather than hold still";

  sample->previous_position = {0.0F, 0.0F, 4.0F};
  sample->position = {0.0F, 0.0F, 5.0F};
  ++sample->tick_sequence;
  ASSERT_TRUE(Render::UnitRenderCache::update_model_matrix(cached, k_quarter * 3.0F));
  EXPECT_NEAR(cached.model_matrix.column(3).z(), 5.0F, 1.0e-4F)
      << "the next sample must continue the same motion, not restart it";
}

TEST_F(UnitRenderCacheTest, ThePresentedStepIsUniformAtANonMultipleDisplayRate) {
  Render::UnitRenderCache cache;

  Engine::Core::StandaloneEntity entity_scratch(43);
  Engine::Core::Entity& entity = entity_scratch.entity();
  entity.add_component<Engine::Core::RenderableComponent>();
  entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  entity.add_component<Engine::Core::TransformComponent>(0.0F, 0.0F, 0.0F);

  auto* sample =
      entity.add_component<Engine::Core::CommanderPresentationSampleComponent>();
  ASSERT_NE(sample, nullptr);
  constexpr float k_tick = 1.0F / 60.0F;
  constexpr float k_speed = 5.375F;
  constexpr float k_step = k_speed * k_tick;
  sample->valid = true;
  sample->snap = false;
  sample->tick_seconds = k_tick;
  sample->tick_sequence = 1;
  sample->previous_position = {0.0F, 0.0F, -k_step};
  sample->position = {0.0F, 0.0F, 0.0F};

  auto& cached = cache.get_or_create(world_view, 43, &entity, 1);
  ASSERT_NE(cached.presentation, nullptr);

  constexpr float k_frame = 1.0F / 144.0F;
  float simulated = 0.0F;
  float presented_time = 0.0F;
  float previous_z = 0.0F;
  bool have_previous = false;
  float slowest = 0.0F;
  float fastest = 0.0F;
  for (int frame = 0; frame < 200; ++frame) {
    presented_time += k_frame;
    while (simulated + k_tick <= presented_time + 1.0e-7F) {
      simulated += k_tick;
      sample->previous_position = sample->position;
      sample->position.z += k_step;
      ++sample->tick_sequence;
    }
    static_cast<void>(Render::UnitRenderCache::update_model_matrix(cached, k_frame));
    float const z = cached.model_matrix.column(3).z();
    if (have_previous && frame > 20) {
      float const speed = (z - previous_z) / k_frame;
      slowest = slowest > 0.0F ? std::min(slowest, speed) : speed;
      fastest = std::max(fastest, speed);
    }
    previous_z = z;
    have_previous = true;
  }

  ASSERT_GT(slowest, 0.0F);
  EXPECT_LT(fastest, slowest * 1.10F)
      << "the presented speed swings between " << slowest << " and " << fastest
      << " m/s while the simulation runs at a constant " << k_speed << " m/s";
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
