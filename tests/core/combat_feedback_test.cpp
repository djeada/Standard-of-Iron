#include <gtest/gtest.h>

#include "app/core/combat_feedback.h"
#include "app/core/focus_target.h"
#include "game/core/component.h"
#include "game/core/world.h"

namespace {

using App::Core::CombatFeedbackStore;
using App::Core::CombatHitFeedback;

auto hit_on(Engine::Core::EntityID target,
            int damage,
            bool incoming = true,
            bool killing = false) -> CombatHitFeedback {
  CombatHitFeedback hit;
  hit.target = target;
  hit.damage = damage;
  hit.damage_ratio = static_cast<float>(damage) / 100.0F;
  hit.incoming = incoming;
  hit.outgoing = !incoming;
  hit.killing_blow = killing;
  return hit;
}

TEST(CombatFeedbackStoreTest, HitsOnTheSameTargetWithinTheWindowCoalesce) {
  CombatFeedbackStore store;
  store.push(hit_on(7, 10));
  store.update(0.03F);
  store.push(hit_on(7, 15));
  store.push(hit_on(9, 5));

  ASSERT_EQ(store.pending().size(), 2U);
  EXPECT_EQ(store.pending().front().damage, 25);
  EXPECT_EQ(store.pending().front().hits, 2);

  EXPECT_TRUE(store.pop_ready().empty()) << "nothing is released before the window";
  store.update(0.2F);
  const auto ready = store.pop_ready();
  ASSERT_EQ(ready.size(), 2U);
  EXPECT_TRUE(store.pending().empty());
}

TEST(CombatFeedbackStoreTest, KillingBlowsAreReleasedImmediatelyAndAbsorbEarlierHits) {
  CombatFeedbackStore store;
  store.push(hit_on(7, 10));
  store.push(hit_on(7, 40, true, true));

  const auto ready = store.pop_ready();
  ASSERT_EQ(ready.size(), 1U);
  EXPECT_TRUE(ready.front().killing_blow);
  EXPECT_EQ(ready.front().damage, 50);
  EXPECT_EQ(ready.front().hits, 2);
}

TEST(CombatFeedbackStoreTest, TheBudgetDropsTheLeastImportantHitFirst) {
  CombatFeedbackStore store(
      CombatFeedbackStore::Limits{.max_pending = 3, .coalesce_window = 0.1F});
  store.push(hit_on(1, 5, false));
  store.push(hit_on(2, 6, false));
  store.push(hit_on(3, 7, false));

  auto focused = hit_on(4, 3, true);
  focused.focused = true;
  store.push(focused);

  ASSERT_EQ(store.pending().size(), 3U);
  bool kept_focused = false;
  bool kept_weakest = false;
  for (const auto& hit : store.pending()) {
    kept_focused = kept_focused || hit.target == 4;
    kept_weakest = kept_weakest || hit.target == 1;
  }
  EXPECT_TRUE(kept_focused) << "a hit on the focused unit outranks unfocused chatter";
  EXPECT_FALSE(kept_weakest) << "the smallest unfocused hit is the one that goes";

  store.push(hit_on(5, 1, false));
  EXPECT_EQ(store.pending().size(), 3U)
      << "a hit weaker than everything pending is dropped";
}

TEST(CombatFeedbackStoreTest, ReadyHitsComeOutMostImportantFirst) {
  CombatFeedbackStore store;
  store.push(hit_on(1, 5, false));
  store.push(hit_on(2, 5, true));
  store.push(hit_on(3, 90, false, true));
  store.update(1.0F);
  const auto ready = store.pop_ready();
  ASSERT_EQ(ready.size(), 3U);
  EXPECT_TRUE(ready[0].killing_blow);
  EXPECT_TRUE(ready[1].incoming);
  EXPECT_EQ(ready[2].target, 1U);
}

TEST(CombatFeedbackStoreTest, VariantCarriesTheFieldsTheOverlayReads) {
  auto hit = hit_on(3, 12, true);
  hit.focused = true;
  hit.hits = 2;
  const auto list = CombatFeedbackStore::to_variant({hit});
  ASSERT_EQ(list.size(), 1);
  const auto map = list.front().toMap();
  EXPECT_EQ(map.value("damage").toInt(), 12);
  EXPECT_TRUE(map.value("incoming").toBool());
  EXPECT_TRUE(map.value("focused").toBool());
  EXPECT_EQ(map.value("hits").toInt(), 2);
  EXPECT_FALSE(map.value("killingBlow").toBool());
}

class FocusTargetTest : public ::testing::Test {
protected:
  auto create(float x,
              int owner,
              bool building = false,
              int health = 100) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, 0.0F);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->owner_id = owner;
    unit->health = health;
    unit->max_health = 100;
    unit->spawn_type =
        building ? Game::Units::SpawnType::Barracks : Game::Units::SpawnType::Archer;
    if (building) {
      entity->add_component<Engine::Core::BuildingComponent>();
    }
    return entity;
  }

  static void attack(Engine::Core::Entity* attacker, Engine::Core::EntityID target) {
    auto* component = attacker->add_component<Engine::Core::AttackTargetComponent>();
    component->target_id = target;
  }

  Engine::Core::World world;
};

TEST_F(FocusTargetTest, ASingleOwnBuildingSelectionIsTheFocus) {
  auto* barracks = create(0.0F, 1, true);
  auto* archer = create(1.0F, 1);
  EXPECT_EQ(App::Core::resolve_focus_entity(&world, {barracks->get_id()}, 0, 1),
            barracks->get_id());
  EXPECT_EQ(App::Core::resolve_focus_entity(&world, {archer->get_id()}, 0, 1), 0U);
  EXPECT_EQ(App::Core::resolve_focus_entity(
                &world, {barracks->get_id(), archer->get_id()}, 0, 1),
            0U);
}

TEST_F(FocusTargetTest, TheInspectedEnemyIsTheFocusUntilItDies) {
  auto* enemy = create(3.0F, 2);
  EXPECT_EQ(App::Core::resolve_focus_entity(&world, {}, enemy->get_id(), 1),
            enemy->get_id());
  enemy->get_component<Engine::Core::UnitComponent>()->health = 0;
  EXPECT_EQ(App::Core::resolve_focus_entity(&world, {}, enemy->get_id(), 1), 0U);
}

TEST_F(FocusTargetTest, PrimaryTargetIsTheOneMostOfTheSelectionIsAttacking) {
  auto* a = create(0.0F, 1);
  auto* b = create(1.0F, 1);
  auto* c = create(2.0F, 1);
  auto* enemy_one = create(5.0F, 2);
  auto* enemy_two = create(6.0F, 2);
  attack(a, enemy_two->get_id());
  attack(b, enemy_one->get_id());
  attack(c, enemy_two->get_id());

  const std::vector<Engine::Core::EntityID> selection{
      a->get_id(), b->get_id(), c->get_id()};
  EXPECT_EQ(App::Core::primary_attack_target(&world, selection), enemy_two->get_id());
  EXPECT_EQ(
      App::Core::count_selection_attacking(&world, selection, enemy_two->get_id()), 2);
  EXPECT_EQ(App::Core::count_units_attacking(&world, enemy_two->get_id(), 1), 2);
  EXPECT_EQ(App::Core::count_units_attacking(&world, enemy_one->get_id(), 1), 1);
}

TEST_F(FocusTargetTest, DeadTargetsAreNotCountedAsPrimary) {
  auto* a = create(0.0F, 1);
  auto* corpse = create(5.0F, 2, false, 0);
  attack(a, corpse->get_id());
  EXPECT_EQ(App::Core::primary_attack_target(&world, {a->get_id()}), 0U);
}

TEST_F(FocusTargetTest, IncomingAttackersAreCountedPerTarget) {
  auto* mine = create(0.0F, 1);
  auto* enemy_a = create(5.0F, 2);
  auto* enemy_b = create(6.0F, 2);
  auto* friendly = create(1.0F, 1);
  attack(enemy_a, mine->get_id());
  attack(enemy_b, mine->get_id());
  attack(friendly, enemy_a->get_id());
  EXPECT_EQ(App::Core::count_enemies_attacking(&world, mine->get_id(), 1), 2);
  EXPECT_EQ(App::Core::count_enemies_attacking(&world, enemy_a->get_id(), 1), 0);
}

TEST_F(FocusTargetTest, VariantExposesTheFieldsTheHudBinds) {
  App::Core::FocusTargetInfo info;
  info.valid = true;
  info.id = 9;
  info.name = QStringLiteral("Barracks");
  info.is_building = true;
  info.is_enemy = true;
  info.health = 40;
  info.max_health = 80;
  info.health_ratio = 0.5;
  info.attacked_by_local = 4;
  const auto map = App::Core::focus_target_to_variant(info);
  EXPECT_TRUE(map.value("valid").toBool());
  EXPECT_EQ(map.value("name").toString(), QStringLiteral("Barracks"));
  EXPECT_TRUE(map.value("isBuilding").toBool());
  EXPECT_TRUE(map.value("isEnemy").toBool());
  EXPECT_DOUBLE_EQ(map.value("healthRatio").toDouble(), 0.5);
  EXPECT_EQ(map.value("attackedByLocal").toInt(), 4);
  EXPECT_FALSE(
      App::Core::building_display_name(Game::Units::SpawnType::Barracks).isEmpty());
  EXPECT_TRUE(
      App::Core::building_display_name(Game::Units::SpawnType::Archer).isEmpty());
}

} // namespace
