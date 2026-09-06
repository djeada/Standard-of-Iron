#include <gtest/gtest.h>
#include <optional>
#include <vector>

#include "game/core/component_core.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "game/mission/commander_voice_observer.h"
#include "game/units/spawn_type.h"

namespace {

constexpr int k_local = 1;
constexpr int k_enemy = 2;
constexpr int k_ally = 3;

using Game::Mission::AttackPlanSource;
using Game::Mission::CommanderVoiceObserver;

class ScriptedPlans final : public AttackPlanSource {
public:
  std::optional<AttackPlan> plan;
  [[nodiscard]] auto attack_plan(int) const -> std::optional<AttackPlan> override {
    return plan;
  }
};

class CommanderVoiceObserverTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_tuning.poll_interval_seconds = 0.5F;
    m_observer.configure({k_local, k_enemy, k_ally}, k_local, m_tuning);

    m_attacks =
        Engine::Core::ScopedEventSubscription<Engine::Core::AiAttackLaunchedEvent>(
            [this](const auto& e) {
              attacks.push_back({e.attacker_owner_id, e.target_owner_id});
            });
    m_sieges =
        Engine::Core::ScopedEventSubscription<Engine::Core::OwnerUnderAttackEvent>(
            [this](const auto& e) {
              sieges.push_back({e.owner_id, e.attacker_owner_id});
            });
    m_contacts =
        Engine::Core::ScopedEventSubscription<Engine::Core::OwnersFirstContactEvent>(
            [this](const auto& e) {
              contacts.push_back({e.owner_id, e.other_owner_id});
            });
    m_losses =
        Engine::Core::ScopedEventSubscription<Engine::Core::OwnerHeavyLossesEvent>(
            [this](const auto& e) {
              losses.push_back({e.owner_id, e.killer_owner_id});
            });
    m_near = Engine::Core::ScopedEventSubscription<Engine::Core::OwnerNearDefeatEvent>(
        [this](const auto& e) { near_defeat.push_back(e.owner_id); });
    m_gone = Engine::Core::ScopedEventSubscription<Engine::Core::OwnerEliminatedEvent>(
        [this](const auto& e) { eliminated.push_back({e.owner_id, e.by_owner_id}); });
  }

  void TearDown() override {
    m_observer.clear();
    Engine::Core::EventManager::instance().clear_all_subscriptions();
  }

  auto spawn(int owner,
             Game::Units::SpawnType type,
             int health = 100) -> Engine::Core::EntityID {
    auto* entity = m_world.create_entity();
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->owner_id = owner;
    unit->spawn_type = type;
    unit->health = health;
    return entity->get_id();
  }

  void kill(Engine::Core::EntityID id, int killer) {
    auto* unit = m_world.try_get<Engine::Core::UnitComponent>(id);
    ASSERT_NE(unit, nullptr);
    unit->health = 0;
    Engine::Core::EventManager::instance().publish(
        Engine::Core::UnitDiedEvent(id, unit->owner_id, unit->spawn_type, 0, killer));
  }

  void hit_building(int owner, int attacker, int damage) {
    Engine::Core::EventManager::instance().publish(Engine::Core::BuildingAttackedEvent(
        1, owner, Game::Units::SpawnType::Barracks, 0, attacker, damage));
  }

  void tick(float seconds, const AttackPlanSource* plans = nullptr) {
    m_observer.update(m_world, plans, seconds);
  }

  struct Pair {
    int a;
    int b;
    auto operator==(const Pair& other) const -> bool {
      return a == other.a && b == other.b;
    }
  };

  Engine::Core::World m_world;
  Game::Mission::CommanderVoiceTuning m_tuning;
  CommanderVoiceObserver m_observer;

  std::vector<Pair> attacks;
  std::vector<Pair> sieges;
  std::vector<Pair> contacts;
  std::vector<Pair> losses;
  std::vector<int> near_defeat;
  std::vector<Pair> eliminated;

  Engine::Core::ScopedEventSubscription<Engine::Core::AiAttackLaunchedEvent> m_attacks;
  Engine::Core::ScopedEventSubscription<Engine::Core::OwnerUnderAttackEvent> m_sieges;
  Engine::Core::ScopedEventSubscription<Engine::Core::OwnersFirstContactEvent>
      m_contacts;
  Engine::Core::ScopedEventSubscription<Engine::Core::OwnerHeavyLossesEvent> m_losses;
  Engine::Core::ScopedEventSubscription<Engine::Core::OwnerNearDefeatEvent> m_near;
  Engine::Core::ScopedEventSubscription<Engine::Core::OwnerEliminatedEvent> m_gone;
};

TEST_F(CommanderVoiceObserverTest, ACommittedWaveIsReportedOnceWithItsTargetOwner) {
  const auto barracks = spawn(k_local, Game::Units::SpawnType::Barracks);
  ScriptedPlans plans;
  plans.plan = AttackPlanSource::AttackPlan{
      .committed = false, .committed_at = -1000.0F, .target_id = barracks};

  tick(1.0F, &plans);
  EXPECT_TRUE(attacks.empty());

  plans.plan->committed = true;
  plans.plan->committed_at = 30.0F;
  tick(1.0F, &plans);
  tick(1.0F, &plans);
  ASSERT_EQ(attacks.size(), 2U) << "both AI owners share the scripted plan";
  EXPECT_EQ(attacks.front().b, k_local);
  EXPECT_TRUE(attacks.front().a == k_enemy || attacks.front().a == k_ally);

  tick(5.0F, &plans);
  EXPECT_EQ(attacks.size(), 2U) << "the same commitment must not be reported again";

  plans.plan->committed_at = 90.0F;
  tick(1.0F, &plans);
  EXPECT_EQ(attacks.size(), 4U) << "a new commitment is a new attack";
}

TEST_F(CommanderVoiceObserverTest, SustainedBuildingDamageReadsAsOneSiegeUntilItCools) {
  for (int i = 0; i < 3; ++i) {
    hit_building(k_enemy, k_local, 30);
    tick(0.1F);
  }
  EXPECT_TRUE(sieges.empty())
      << "ninety damage in three hits is a skirmish, not a siege";

  hit_building(k_enemy, k_local, 40);
  tick(0.1F);
  ASSERT_EQ(sieges.size(), 1U);
  EXPECT_EQ(sieges.front(), (Pair{k_enemy, k_local}));

  for (int i = 0; i < 10; ++i) {
    hit_building(k_enemy, k_local, 50);
    tick(1.0F);
  }
  EXPECT_EQ(sieges.size(), 1U) << "a siege in progress is not announced twice";

  tick(m_tuning.under_attack_cooloff_seconds + 1.0F);
  for (int i = 0; i < 6; ++i) {
    hit_building(k_enemy, k_local, 10);
    tick(0.5F);
  }
  EXPECT_EQ(sieges.size(), 2U) << "after the cool-off a fresh assault counts again";

  hit_building(k_enemy, k_enemy, 500);
  tick(0.1F);
  EXPECT_EQ(sieges.size(), 2U) << "an owner cannot besiege itself";
}

TEST_F(CommanderVoiceObserverTest, FirstContactFiresOncePerPairAndFacesThePlayer) {
  Engine::Core::EventManager::instance().publish(Engine::Core::CombatHitEvent(
      1, 2, 5, Game::Units::SpawnType::Spearman, false, k_local, k_enemy));
  tick(0.1F);
  ASSERT_EQ(contacts.size(), 1U);
  EXPECT_EQ(contacts.front(), (Pair{k_enemy, k_local}))
      << "the AI is the owner even when the player struck first";

  Engine::Core::EventManager::instance().publish(Engine::Core::CombatHitEvent(
      3, 4, 5, Game::Units::SpawnType::Spearman, false, k_enemy, k_local));
  tick(0.1F);
  EXPECT_EQ(contacts.size(), 1U) << "the same pair in the other direction is not new";

  Engine::Core::EventManager::instance().publish(Engine::Core::CombatHitEvent(
      5, 6, 5, Game::Units::SpawnType::Spearman, false, k_ally, k_enemy));
  tick(0.1F);
  ASSERT_EQ(contacts.size(), 2U);
  EXPECT_EQ(contacts.back(), (Pair{k_ally, k_enemy}));
}

TEST_F(CommanderVoiceObserverTest, HeavyLossesNeedAWindowfulOfDeathsAndNameTheKiller) {
  std::vector<Engine::Core::EntityID> troops;
  for (int i = 0; i < 12; ++i) {
    troops.push_back(spawn(k_local, Game::Units::SpawnType::Spearman));
  }
  tick(1.0F);

  for (int i = 0; i < 5; ++i) {
    kill(troops[static_cast<std::size_t>(i)], k_enemy);
    tick(1.0F);
  }
  EXPECT_TRUE(losses.empty()) << "five deaths is under the floor of six";

  kill(troops[5], k_ally);
  tick(1.0F);
  ASSERT_EQ(losses.size(), 1U);
  EXPECT_EQ(losses.front(), (Pair{k_local, k_enemy})) << "the majority killer is named";

  for (int i = 6; i < 12; ++i) {
    kill(troops[static_cast<std::size_t>(i)], k_enemy);
    tick(1.0F);
  }
  EXPECT_EQ(losses.size(), 1U) << "the beat re-arms only after a minute";
}

TEST_F(CommanderVoiceObserverTest, NearDefeatArmsAtAPeakFiresAtTheLastStandAndReArms) {
  std::vector<Engine::Core::EntityID> troops;
  const auto barracks = spawn(k_enemy, Game::Units::SpawnType::Barracks);
  for (int i = 0; i < 3; ++i) {
    troops.push_back(spawn(k_enemy, Game::Units::SpawnType::Spearman));
  }
  tick(1.0F);
  EXPECT_TRUE(near_defeat.empty())
      << "three men who never had an army are not a last stand";

  for (int i = 0; i < 9; ++i) {
    troops.push_back(spawn(k_enemy, Game::Units::SpawnType::Spearman));
  }
  tick(1.0F);
  for (std::size_t i = 0; i < troops.size() - 3; ++i) {
    auto* unit = m_world.try_get<Engine::Core::UnitComponent>(troops[i]);
    unit->health = 0;
  }
  tick(1.0F);
  ASSERT_EQ(near_defeat.size(), 1U);
  EXPECT_EQ(near_defeat.front(), k_enemy);

  tick(5.0F);
  EXPECT_EQ(near_defeat.size(), 1U) << "a last stand is announced once";

  for (int i = 0; i < 6; ++i) {
    spawn(k_enemy, Game::Units::SpawnType::Spearman);
  }
  tick(1.0F);
  std::vector<Engine::Core::EntityID> standing;
  for (auto [id, unit] : m_world.view<Engine::Core::UnitComponent>()) {
    if (unit.owner_id == k_enemy && id != barracks) {
      standing.push_back(id);
    }
  }
  for (const auto id : standing) {
    m_world.try_get<Engine::Core::UnitComponent>(id)->health = 0;
  }
  tick(1.0F);
  EXPECT_EQ(near_defeat.size(), 2U)
      << "a recovered owner can fall to a last stand again";

  auto* keep = m_world.try_get<Engine::Core::UnitComponent>(barracks);
  keep->health = 0;
  Engine::Core::EventManager::instance().publish(Engine::Core::UnitDiedEvent(
      barracks, k_enemy, Game::Units::SpawnType::Barracks, 0, k_local));
  tick(1.0F);
  ASSERT_EQ(eliminated.size(), 1U);
  EXPECT_EQ(eliminated.front(), (Pair{k_enemy, k_local}));
  tick(5.0F);
  EXPECT_EQ(eliminated.size(), 1U);
}

TEST_F(CommanderVoiceObserverTest, RestoreKeepsContactsAndCommitmentsFromReplaying) {
  const auto barracks = spawn(k_local, Game::Units::SpawnType::Barracks);
  ScriptedPlans plans;
  plans.plan = AttackPlanSource::AttackPlan{
      .committed = true, .committed_at = 30.0F, .target_id = barracks};
  Engine::Core::EventManager::instance().publish(Engine::Core::CombatHitEvent(
      1, 2, 5, Game::Units::SpawnType::Spearman, false, k_local, k_enemy));
  tick(1.0F, &plans);
  ASSERT_EQ(attacks.size(), 2U);
  ASSERT_EQ(contacts.size(), 1U);

  const QJsonObject state = m_observer.serialize();
  m_observer.configure({k_local, k_enemy, k_ally}, k_local, m_tuning);
  m_observer.restore(state);

  Engine::Core::EventManager::instance().publish(Engine::Core::CombatHitEvent(
      1, 2, 5, Game::Units::SpawnType::Spearman, false, k_local, k_enemy));
  tick(1.0F, &plans);
  EXPECT_EQ(attacks.size(), 2U) << "the restored commitment stamp stops a replay";
  EXPECT_EQ(contacts.size(), 1U) << "first contact survived the save";
}

} // namespace
