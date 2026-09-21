#include <chrono>
#include <gtest/gtest.h>
#include <thread>

#include "game/core/component_core.h"
#include "game/core/world.h"
#include "game/map/visibility_service.h"
#include "game/session/session_context.h"

namespace {

using Game::Map::VisibilityService;
using Game::Map::VisibilityState;

constexpr int k_player = 1;

constexpr int k_grid = 64;
constexpr int k_centre = 32;
constexpr int k_far = 57;
constexpr float k_short_sight = 13.0F;
constexpr float k_long_sight = 40.0F;

auto spawn_scout(Engine::Core::World& world,
                 float x,
                 float z,
                 float vision) -> Engine::Core::EntityID {
  auto* entity = world.create_entity();
  auto* transform = entity->add_component<Engine::Core::TransformComponent>();
  transform->position.x = x;
  transform->position.z = z;
  auto* unit = entity->add_component<Engine::Core::UnitComponent>();
  unit->owner_id = k_player;
  unit->health = 100;
  unit->max_health = 100;
  unit->vision_range = vision;
  return entity->get_id();
}

auto settle_until(Engine::Core::World& world,
                  int x,
                  int z,
                  VisibilityState wanted) -> bool {
  auto& visibility = VisibilityService::instance();
  for (int attempt = 0; attempt < 200; ++attempt) {
    (void)visibility.update(world, k_player);
    if (visibility.state_at(x, z) == wanted) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return visibility.state_at(x, z) == wanted;
}

class VisibilityInvalidationTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_session = std::make_unique<Game::Session::SessionContext>();
    m_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
    VisibilityService::instance().initialize(k_grid, k_grid, 1.0F);
  }

  void TearDown() override {
    VisibilityService::instance().reset();
    m_scope.reset();
    m_session.reset();
  }

  [[nodiscard]] auto world() -> Engine::Core::World& { return m_session->world(); }

  std::unique_ptr<Game::Session::SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
};

TEST_F(VisibilityInvalidationTest, TheFogDimsWhenTheLastScoutDies) {
  auto& visibility = VisibilityService::instance();
  const auto scout = spawn_scout(world(), 0.0F, 0.0F, k_short_sight);

  visibility.compute_immediate(world(), k_player);
  ASSERT_EQ(visibility.state_at(k_centre, k_centre), VisibilityState::Visible)
      << "the scout never lit its own cell, so this proves nothing";

  world().destroy_entity(scout);

  EXPECT_TRUE(settle_until(world(), k_centre, k_centre, VisibilityState::Explored))
      << "the cell stayed lit by a unit that no longer exists";
  EXPECT_NE(visibility.state_at(k_centre, k_centre), VisibilityState::Unseen)
      << "what was seen stays remembered";
}

TEST_F(VisibilityInvalidationTest, AStationaryScoutThatSeesFurtherLightsMore) {
  auto& visibility = VisibilityService::instance();
  const auto scout = spawn_scout(world(), 0.0F, 0.0F, k_short_sight);

  visibility.compute_immediate(world(), k_player);
  ASSERT_EQ(visibility.state_at(k_centre, k_centre), VisibilityState::Visible);
  const auto far_cell_before = visibility.state_at(k_far, k_centre);

  auto* unit = world().try_get<Engine::Core::UnitComponent>(scout);
  ASSERT_NE(unit, nullptr);
  unit->vision_range = k_long_sight;

  EXPECT_TRUE(settle_until(world(), k_far, k_centre, VisibilityState::Visible))
      << "the scout's new range never reached the fog; it was "
      << static_cast<int>(far_cell_before) << " before and "
      << static_cast<int>(visibility.state_at(k_far, k_centre)) << " after";
}

TEST_F(VisibilityInvalidationTest, AStationaryScoutThatSeesLessGivesGroundBack) {
  auto& visibility = VisibilityService::instance();
  const auto scout = spawn_scout(world(), 0.0F, 0.0F, k_long_sight);

  visibility.compute_immediate(world(), k_player);
  ASSERT_EQ(visibility.state_at(k_far, k_centre), VisibilityState::Visible);

  auto* unit = world().try_get<Engine::Core::UnitComponent>(scout);
  ASSERT_NE(unit, nullptr);
  unit->vision_range = k_short_sight;

  EXPECT_TRUE(settle_until(world(), k_far, k_centre, VisibilityState::Explored))
      << "a scout that can no longer see that far still lights it";
  EXPECT_EQ(visibility.state_at(k_centre, k_centre), VisibilityState::Visible)
      << "the scout stopped lighting its own cell";
}

} // namespace
