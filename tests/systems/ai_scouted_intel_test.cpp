#include <algorithm>
#include <gtest/gtest.h>
#include <memory>

#include "game/core/component_commander.h"
#include "game/core/component_core.h"
#include "game/core/component_structures.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/systems/ai_system/ai_snapshot_builder.h"
#include "game/systems/ai_system/ai_types.h"
#include "game/systems/owner_registry.h"

namespace {

namespace AISnapshotBuilder = Game::Systems::AI::AISnapshotBuilder;
using Game::Systems::AI::KnownObjectives;

constexpr int k_thinker = 2;
constexpr int k_foe = 3;

auto place(Engine::Core::World& world, int owner_id, float x, float z, float vision)
    -> Engine::Core::EntityID {
  auto* entity = world.create_entity();
  auto* transform = entity->add_component<Engine::Core::TransformComponent>();
  transform->position.x = x;
  transform->position.z = z;
  auto* unit = entity->add_component<Engine::Core::UnitComponent>();
  unit->owner_id = owner_id;
  unit->health = 100;
  unit->max_health = 100;
  unit->vision_range = vision;
  return entity->get_id();
}

auto place_barracks(Engine::Core::World& world,
                    int owner_id,
                    float x,
                    float z) -> Engine::Core::EntityID {
  const auto id = place(world, owner_id, x, z, 1.0F);
  world.emplace<Engine::Core::BuildingComponent>(id);
  auto* unit = world.try_get<Engine::Core::UnitComponent>(id);
  unit->spawn_type = Game::Units::SpawnType::Barracks;
  return id;
}

auto knows_about(const Game::Systems::AI::AISnapshot& snapshot,
                 Engine::Core::EntityID id) -> bool {
  return std::any_of(snapshot.strategic_objectives.begin(),
                     snapshot.strategic_objectives.end(),
                     [id](const auto& objective) { return objective.id == id; });
}

class AiScoutedIntelTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_session = std::make_unique<Game::Session::SessionContext>();
    m_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
    auto& owners = m_session->owners();
    owners.register_owner_with_id(k_thinker, Game::Systems::OwnerType::AI, "thinker");
    owners.register_owner_with_id(k_foe, Game::Systems::OwnerType::AI, "foe");
    owners.set_owner_team(k_thinker, 1);
    owners.set_owner_team(k_foe, 2);
  }

  void TearDown() override {
    m_scope.reset();
    m_session.reset();
  }

  [[nodiscard]] auto world() -> Engine::Core::World& { return m_session->world(); }

  std::unique_ptr<Game::Session::SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
};

TEST_F(AiScoutedIntelTest, ABaseNobodyHasScoutedIsNotAnObjective) {
  (void)place(world(), k_thinker, 0.0F, 0.0F, 6.0F);
  const auto hidden_base = place_barracks(world(), k_foe, 400.0F, 400.0F);

  KnownObjectives known;
  const auto snapshot = AISnapshotBuilder::build(world(), k_thinker, &known);

  EXPECT_FALSE(knows_about(snapshot, hidden_base))
      << "the planner was given a base nothing has ever seen";
  EXPECT_TRUE(known.empty());
}

TEST_F(AiScoutedIntelTest, ABaseInSightIsAnObjectiveAndIsRemembered) {
  (void)place(world(), k_thinker, 0.0F, 0.0F, 40.0F);
  const auto seen_base = place_barracks(world(), k_foe, 5.0F, 5.0F);

  KnownObjectives known;
  const auto snapshot = AISnapshotBuilder::build(world(), k_thinker, &known);

  EXPECT_TRUE(knows_about(snapshot, seen_base))
      << "a base in plain sight was not an objective";
  EXPECT_TRUE(known.contains(seen_base)) << "seeing it taught the AI nothing";
}

TEST_F(AiScoutedIntelTest, AScoutedBaseIsRememberedAfterTheScoutWithdraws) {
  const auto scout = place(world(), k_thinker, 0.0F, 0.0F, 40.0F);
  const auto base = place_barracks(world(), k_foe, 5.0F, 5.0F);

  KnownObjectives known;
  (void)AISnapshotBuilder::build(world(), k_thinker, &known);
  ASSERT_TRUE(known.contains(base));

  auto* transform = world().try_get<Engine::Core::TransformComponent>(scout);
  ASSERT_NE(transform, nullptr);
  transform->position.x = 500.0F;
  transform->position.z = 500.0F;

  const auto after = AISnapshotBuilder::build(world(), k_thinker, &known);
  EXPECT_TRUE(knows_about(after, base))
      << "the opponent forgot a base it had already scouted";
  EXPECT_TRUE(std::none_of(after.visible_enemies.begin(),
                           after.visible_enemies.end(),
                           [base](const auto& contact) { return contact.id == base; }))
      << "an out-of-sight base is remembered intel, not a live contact";
}

TEST_F(AiScoutedIntelTest, ARazedBaseStopsBeingAnObjective) {
  (void)place(world(), k_thinker, 0.0F, 0.0F, 40.0F);
  const auto base = place_barracks(world(), k_foe, 5.0F, 5.0F);

  KnownObjectives known;
  (void)AISnapshotBuilder::build(world(), k_thinker, &known);
  ASSERT_TRUE(known.contains(base));

  world().destroy_entity(base);

  const auto after = AISnapshotBuilder::build(world(), k_thinker, &known);
  EXPECT_FALSE(knows_about(after, base))
      << "the planner still holds a base that has been razed";
  EXPECT_FALSE(known.contains(base)) << "the memory kept a ghost";
}

} // namespace
