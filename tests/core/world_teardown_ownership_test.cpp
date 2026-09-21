#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "game/core/component_core.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/systems/building_collision_registry.h"

namespace {

auto register_building(Game::Session::SessionContext& session,
                       float x,
                       float z) -> Engine::Core::EntityID {
  auto* entity = session.world().create_entity();
  auto* transform = entity->add_component<Engine::Core::TransformComponent>();
  transform->position.x = x;
  transform->position.z = z;
  session.building_collision().register_building(
      entity->get_id(), std::string("barracks"), x, z, 1);
  return entity->get_id();
}

auto footprint_count(Game::Session::SessionContext& session) -> int {
  return static_cast<int>(session.building_collision().get_all_buildings().size());
}

TEST(WorldTeardownOwnership, DestroyingAnEntityOnlyTouchesItsOwnWorld) {
  auto first = std::make_unique<Game::Session::SessionContext>();
  auto second = std::make_unique<Game::Session::SessionContext>();

  const auto in_first = register_building(*first, 10.0F, 10.0F);
  const auto in_second = register_building(*second, 20.0F, 20.0F);
  ASSERT_EQ(in_first, in_second) << "the two worlds did not reuse the same id, so "
                                    "this test proves nothing";
  ASSERT_EQ(footprint_count(*first), 1);
  ASSERT_EQ(footprint_count(*second), 1);

  {
    const Game::Session::ScopedSession scope(*second);
    first->world().destroy_entity(in_first);
  }

  EXPECT_EQ(footprint_count(*first), 0)
      << "the world that lost the building still has its footprint";
  EXPECT_EQ(footprint_count(*second), 1)
      << "destroying an entity in one world removed another world's building";
}

TEST(WorldTeardownOwnership, ClearingAWorldReportsTheEntitiesItDestroys) {
  auto session = std::make_unique<Game::Session::SessionContext>();
  const Game::Session::ScopedSession scope(*session);

  (void)register_building(*session, 5.0F, 5.0F);
  (void)register_building(*session, 9.0F, 9.0F);
  ASSERT_EQ(footprint_count(*session), 2);

  int reported = 0;
  const auto handle = session->world().add_entity_destroyed_observer(
      [&reported](Engine::Core::EntityID) { ++reported; });

  session->world().clear();

  EXPECT_EQ(reported, 2) << "clear() destroyed entities without saying so";
  EXPECT_EQ(footprint_count(*session), 0)
      << "the collision registry outlived the entities it was built from";

  session->world().remove_entity_destroyed_observer(handle);
}

TEST(WorldTeardownOwnership, AScopedSubscriptionLeavesTheBusItJoined) {
  auto first = std::make_unique<Game::Session::SessionContext>();
  auto second = std::make_unique<Game::Session::SessionContext>();

  int seen = 0;
  {
    std::unique_ptr<
        Engine::Core::ScopedEventSubscription<Engine::Core::UnitSelectedEvent>>
        subscription;
    {
      const Game::Session::ScopedSession scope(*first);
      subscription = std::make_unique<
          Engine::Core::ScopedEventSubscription<Engine::Core::UnitSelectedEvent>>(
          [&seen](const Engine::Core::UnitSelectedEvent&) { ++seen; });
      first->events().publish(Engine::Core::UnitSelectedEvent(1));
      ASSERT_EQ(seen, 1) << "the handler never ran on the bus it joined";
    }

    const Game::Session::ScopedSession scope(*second);
    subscription.reset();
  }

  const int before = seen;
  first->events().publish(Engine::Core::UnitSelectedEvent(2));
  EXPECT_EQ(seen, before)
      << "the handler is still attached to the bus it was supposed to leave";
}

} // namespace
