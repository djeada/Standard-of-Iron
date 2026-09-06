#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "game/core/component_presentation.h"
#include "game/core/world.h"
#include "game/session/session_context.h"

namespace {

using Engine::Core::Entity;
using Engine::Core::FormationPresentationComponent;
using Engine::Core::FormationSoldierPresentation;
using Engine::Core::RenderableComponent;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Engine::Core::World;

auto add_squad(World& world, std::size_t soldiers) -> Engine::Core::EntityID {
  auto* entity = world.create_entity();
  entity->add_component<TransformComponent>();
  entity->add_component<UnitComponent>();
  entity->add_component<RenderableComponent>();
  auto* presentation = entity->add_component<FormationPresentationComponent>();
  presentation->soldiers.resize(soldiers);
  presentation->revision = 1;
  return entity->get_id();
}

} // namespace

TEST(RenderPublicationTest, AnUnchangedFormationLayoutIsNotCopiedAgain) {
  Game::Session::SessionContext session;
  const Game::Session::ScopedSession scope(session);
  World& world = session.world();
  world.request_render_snapshots(true);

  const auto squad = add_squad(world, 40);
  world.update(1.0F / 60.0F);
  const auto after_first = world.render_publication_stats();
  ASSERT_GT(after_first.publications, 0U);

  auto* entity = world.get_entity(squad);
  ASSERT_NE(entity, nullptr);
  auto* presentation = entity->get_component<FormationPresentationComponent>();
  ASSERT_NE(presentation, nullptr);
  auto* transform = entity->get_component<TransformComponent>();
  ASSERT_NE(transform, nullptr);

  for (int frame = 0; frame < 5; ++frame) {
    transform->position.x += 1.0F;
    world.update(1.0F / 60.0F);
  }

  const auto after_moving = world.render_publication_stats();
  EXPECT_GT(after_moving.entities_copied, after_first.entities_copied)
      << "a moving squad must still be republished";
  EXPECT_GT(after_moving.revisioned_components_skipped,
            after_first.revisioned_components_skipped)
      << "its soldier layout did not change, so it must not be copied again";
}

TEST(RenderPublicationTest, ChangingTheLayoutRevisionCopiesItAgain) {
  Game::Session::SessionContext session;
  const Game::Session::ScopedSession scope(session);
  World& world = session.world();
  world.request_render_snapshots(true);

  const auto squad = add_squad(world, 8);
  world.update(1.0F / 60.0F);

  auto* presentation =
      world.get_entity(squad)->get_component<FormationPresentationComponent>();
  ASSERT_NE(presentation, nullptr);

  const auto before = world.render_publication_stats().revisioned_components_skipped;
  presentation->soldiers.resize(12);
  ++presentation->revision;
  world.update(1.0F / 60.0F);

  auto snapshot = world.acquire_render_snapshot();
  ASSERT_NE(snapshot, nullptr);
  auto* copied = snapshot->get_entity(squad);
  ASSERT_NE(copied, nullptr);
  const auto* copied_presentation =
      copied->get_component<FormationPresentationComponent>();
  ASSERT_NE(copied_presentation, nullptr);
  EXPECT_EQ(copied_presentation->soldiers.size(), 12U)
      << "a bumped revision must reach the renderer";
  EXPECT_EQ(world.render_publication_stats().revisioned_components_skipped, before);
}

TEST(RenderPublicationTest, PublicationIsSkippedRatherThanAllocatingAnotherWorld) {
  Game::Session::SessionContext session;
  const Game::Session::ScopedSession scope(session);
  World& world = session.world();
  world.request_render_snapshots(true);

  add_squad(world, 4);

  std::vector<std::shared_ptr<World>> retained;
  for (int frame = 0; frame < 8; ++frame) {
    world.update(1.0F / 60.0F);
    retained.push_back(world.acquire_render_snapshot());
  }

  const auto stats = world.render_publication_stats();
  EXPECT_GT(stats.skipped_publications, 0U)
      << "a reader holding every buffer must apply back-pressure, not force a new "
         "World to be allocated";
  EXPECT_LE(stats.publications, 8U);
}
