#include <gtest/gtest.h>

#include "core/component_economy.h"
#include "core/entity.h"
#include "core/ownership_constants.h"
#include "core/world.h"
#include "session/session_context.h"
#include "systems/combat_system/target_rules.h"
#include "systems/owner_registry.h"
#include "wildlife/wildlife_species.h"

using namespace Engine::Core;
using Game::Systems::Combat::EngagementIntent;
using Game::Systems::Combat::evaluate_target;
using Game::Systems::Combat::TargetQuery;
using Game::Systems::Combat::TargetRefusal;

namespace {

constexpr int k_local_owner = 1;
constexpr int k_enemy_owner = 2;
constexpr int k_allied_owner = 3;

constexpr TargetQuery k_ordered{.intent = EngagementIntent::Ordered,
                                .allow_buildings = true};
constexpr TargetQuery k_auto{.intent = EngagementIntent::AutoAcquired,
                             .allow_buildings = true};

class TargetRulesTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_scope = std::make_unique<Game::Session::ScopedSession>(m_session);
    auto& owners = m_session.owners();
    owners.register_owner_with_id(
        k_local_owner, Game::Systems::OwnerType::Player, "me");
    owners.register_owner_with_id(k_enemy_owner, Game::Systems::OwnerType::AI, "them");
    owners.register_owner_with_id(
        k_allied_owner, Game::Systems::OwnerType::AI, "friend");
    owners.set_owner_team(k_local_owner, 1);
    owners.set_owner_team(k_allied_owner, 1);
    owners.set_owner_team(k_enemy_owner, 2);
  }

  auto spawn(int owner_id) -> Entity* {
    auto* entity = m_session.world().create_entity();
    entity->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
    auto* unit = entity->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
    unit->owner_id = owner_id;
    return entity;
  }

  auto spawn_animal(Game::Wildlife::Species species, bool hostile) -> Entity* {
    auto* entity = spawn(Game::Core::NEUTRAL_OWNER_ID);
    auto* wildlife = entity->add_component<WildlifeComponent>();
    wildlife->species = species;
    wildlife->hostile_timer = hostile ? 4.0F : 0.0F;
    return entity;
  }

  [[nodiscard]] auto owners() const -> const Game::Systems::OwnerRegistry& {
    return m_session.owners();
  }

  Game::Session::SessionContext m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
};

TEST_F(TargetRulesTest, AMissingTargetIsRefused) {
  EXPECT_EQ(evaluate_target(owners(), k_local_owner, nullptr, k_ordered),
            TargetRefusal::NoTarget);
}

TEST_F(TargetRulesTest, ADeadTargetIsRefused) {
  auto* enemy = spawn(k_enemy_owner);
  enemy->get_component<UnitComponent>()->health = 0;
  EXPECT_EQ(evaluate_target(owners(), k_local_owner, enemy, k_ordered),
            TargetRefusal::NoTarget);
}

TEST_F(TargetRulesTest, OwnAndAlliedUnitsAreRefusedByBothIntents) {
  auto* mine = spawn(k_local_owner);
  auto* friendly = spawn(k_allied_owner);
  EXPECT_EQ(evaluate_target(owners(), k_local_owner, mine, k_ordered),
            TargetRefusal::SelfOrAllied);
  EXPECT_EQ(evaluate_target(owners(), k_local_owner, friendly, k_ordered),
            TargetRefusal::SelfOrAllied);
  EXPECT_EQ(evaluate_target(owners(), k_local_owner, mine, k_auto),
            TargetRefusal::SelfOrAllied);
  EXPECT_EQ(evaluate_target(owners(), k_local_owner, friendly, k_auto),
            TargetRefusal::SelfOrAllied);
}

TEST_F(TargetRulesTest, AnEnemyUnitIsValidForBothIntents) {
  auto* enemy = spawn(k_enemy_owner);
  EXPECT_EQ(evaluate_target(owners(), k_local_owner, enemy, k_ordered),
            TargetRefusal::None);
  EXPECT_EQ(evaluate_target(owners(), k_local_owner, enemy, k_auto),
            TargetRefusal::None);
}

TEST_F(TargetRulesTest, AHostileWolfIsValidForBothIntents) {
  auto* wolf = spawn_animal(Game::Wildlife::Species::Wolf, true);
  EXPECT_EQ(evaluate_target(owners(), k_local_owner, wolf, k_ordered),
            TargetRefusal::None);
  EXPECT_EQ(evaluate_target(owners(), k_local_owner, wolf, k_auto),
            TargetRefusal::None);
}

TEST_F(TargetRulesTest, APassiveAnimalIsOrderableButNeverAutoAcquired) {
  auto* sheep = spawn_animal(Game::Wildlife::Species::Sheep, false);
  auto* calm_wolf = spawn_animal(Game::Wildlife::Species::Wolf, false);

  EXPECT_EQ(evaluate_target(owners(), k_local_owner, sheep, k_ordered),
            TargetRefusal::None);
  EXPECT_EQ(evaluate_target(owners(), k_local_owner, calm_wolf, k_ordered),
            TargetRefusal::None);

  EXPECT_EQ(evaluate_target(owners(), k_local_owner, sheep, k_auto),
            TargetRefusal::Passive);
  EXPECT_EQ(evaluate_target(owners(), k_local_owner, calm_wolf, k_auto),
            TargetRefusal::Passive);
}

TEST_F(TargetRulesTest, StructuresAreRefusedOnlyWhenTheQueryExcludesThem) {
  auto* keep = spawn(k_enemy_owner);
  keep->add_component<BuildingComponent>();

  EXPECT_EQ(evaluate_target(owners(), k_local_owner, keep, k_ordered),
            TargetRefusal::None);
  EXPECT_EQ(
      evaluate_target(owners(),
                      k_local_owner,
                      keep,
                      {.intent = EngagementIntent::Ordered, .allow_buildings = false}),
      TargetRefusal::Structure);
}

TEST(TargetRefusalKeys, EveryRefusalHasAStableName) {
  using Game::Systems::Combat::target_refusal_key;
  EXPECT_EQ(target_refusal_key(TargetRefusal::None), "valid");
  EXPECT_EQ(target_refusal_key(TargetRefusal::NoTarget), "no_target");
  EXPECT_EQ(target_refusal_key(TargetRefusal::SelfOrAllied), "self_or_allied");
  EXPECT_EQ(target_refusal_key(TargetRefusal::Passive), "passive");
  EXPECT_EQ(target_refusal_key(TargetRefusal::Structure), "structure");
}

TEST_F(TargetRulesTest, HostileContactsSeeHostileWildlifeButNotAHerd) {
  auto* enemy = spawn(k_enemy_owner);
  auto* wolf = spawn_animal(Game::Wildlife::Species::Wolf, true);
  auto* sheep = spawn_animal(Game::Wildlife::Species::Sheep, false);
  auto* friendly = spawn(k_allied_owner);

  const auto contacts =
      Game::Systems::Combat::collect_hostile_contacts(m_session.world(), k_local_owner);

  const auto holds = [&contacts](Entity* entity) {
    return std::find(contacts.begin(), contacts.end(), entity) != contacts.end();
  };
  EXPECT_TRUE(holds(enemy));
  EXPECT_TRUE(holds(wolf));
  EXPECT_FALSE(holds(sheep));
  EXPECT_FALSE(holds(friendly));
}

} // namespace
