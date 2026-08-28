#include <gtest/gtest.h>

#include "app/world/focus_target.h"
#include "app/world/world_feedback.h"
#include "game/core/component.h"
#include "game/core/world.h"

namespace {

using App::Core::FeedbackKind;
using App::Core::FeedbackStyle;
using App::Core::WorldFeedbackStore;
using App::Core::WorldFeedbackTick;

auto hit_on(Engine::Core::EntityID target,
            int damage,
            bool incoming = true,
            bool killing = false) -> WorldFeedbackTick {
  WorldFeedbackTick hit;
  hit.anchor = target;
  hit.kind = FeedbackKind::Damage;
  hit.amount = damage;
  hit.severity = static_cast<float>(damage) / 100.0F;
  hit.incoming = incoming;
  hit.outgoing = !incoming;
  hit.killing_blow = killing;
  return hit;
}

auto resource_on(Engine::Core::EntityID anchor,
                 int resource,
                 int amount) -> WorldFeedbackTick {
  WorldFeedbackTick tick;
  tick.anchor = anchor;
  tick.kind = FeedbackKind::Resource;
  tick.resource = resource;
  tick.amount = amount;
  return tick;
}

TEST(WorldFeedbackStoreTest, HitsOnTheSameTargetWithinTheWindowCoalesce) {
  WorldFeedbackStore store;
  store.push(hit_on(7, 10));
  store.update(0.03F);
  store.push(hit_on(7, 15));
  store.push(hit_on(9, 5));

  ASSERT_EQ(store.pending().size(), 2U);
  EXPECT_EQ(store.pending().front().amount, 25);
  EXPECT_EQ(store.pending().front().hits, 2);

  EXPECT_TRUE(store.pop_ready().empty()) << "nothing is released before the window";
  store.update(0.2F);
  const auto ready = store.pop_ready();
  ASSERT_EQ(ready.size(), 2U);
  EXPECT_TRUE(store.pending().empty());
}

TEST(WorldFeedbackStoreTest, KillingBlowsAreReleasedImmediatelyAndAbsorbEarlierHits) {
  WorldFeedbackStore store;
  store.push(hit_on(7, 10));
  store.push(hit_on(7, 40, true, true));

  const auto ready = store.pop_ready();
  ASSERT_EQ(ready.size(), 1U);
  EXPECT_TRUE(ready.front().killing_blow);
  EXPECT_EQ(ready.front().amount, 50);
  EXPECT_EQ(ready.front().hits, 2);
}

TEST(WorldFeedbackStoreTest, TheBudgetDropsTheLeastImportantHitFirst) {
  WorldFeedbackStore store(WorldFeedbackStore::Limits{.max_pending_per_kind = 3,
                                                      .damage_coalesce_window = 0.1F,
                                                      .economy_coalesce_window = 0.4F});
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
    kept_focused = kept_focused || hit.anchor == 4;
    kept_weakest = kept_weakest || hit.anchor == 1;
  }
  EXPECT_TRUE(kept_focused) << "a hit on the focused unit outranks unfocused chatter";
  EXPECT_FALSE(kept_weakest) << "the smallest unfocused hit is the one that goes";

  store.push(hit_on(5, 1, false));
  EXPECT_EQ(store.pending().size(), 3U)
      << "a hit weaker than everything pending is dropped";
}

TEST(WorldFeedbackStoreTest, ReadyHitsComeOutMostImportantFirst) {
  WorldFeedbackStore store;
  store.push(hit_on(1, 5, false));
  store.push(hit_on(2, 5, true));
  store.push(hit_on(3, 90, false, true));
  store.update(1.0F);
  const auto ready = store.pop_ready();
  ASSERT_EQ(ready.size(), 3U);
  EXPECT_TRUE(ready[0].killing_blow);
  EXPECT_TRUE(ready[1].incoming);
  EXPECT_EQ(ready[2].anchor, 1U);
}

TEST(WorldFeedbackStoreTest, VariantCarriesTheFieldsTheOverlayReads) {
  auto hit = hit_on(3, 12, true);
  hit.focused = true;
  hit.hits = 2;
  const auto list = WorldFeedbackStore::to_variant({hit});
  ASSERT_EQ(list.size(), 1);
  const auto map = list.front().toMap();
  EXPECT_EQ(map.value("amount").toInt(), 12);
  EXPECT_EQ(map.value("kind").toString(), QStringLiteral("damage"));
  EXPECT_EQ(map.value("style").toString(), QStringLiteral("tick"));
  EXPECT_TRUE(map.value("incoming").toBool());
  EXPECT_TRUE(map.value("focused").toBool());
  EXPECT_EQ(map.value("hits").toInt(), 2);
  EXPECT_FALSE(map.value("killingBlow").toBool());
}

TEST(WorldFeedbackStoreTest, CommanderBurstsOutrankRtsChatterOfTheSameDamage) {
  WorldFeedbackStore store(WorldFeedbackStore::Limits{.max_pending_per_kind = 1,
                                                      .damage_coalesce_window = 0.1F,
                                                      .economy_coalesce_window = 0.4F});
  store.push(hit_on(1, 40, false));

  auto burst = hit_on(2, 5, false);
  burst.style = FeedbackStyle::Burst;
  store.push(burst);

  ASSERT_EQ(store.pending().size(), 1U);
  EXPECT_EQ(store.pending().front().style, FeedbackStyle::Burst);
}

TEST(WorldFeedbackStoreTest, EconomyTicksNeverEvictCombatTicks) {
  WorldFeedbackStore store(WorldFeedbackStore::Limits{.max_pending_per_kind = 2,
                                                      .damage_coalesce_window = 0.1F,
                                                      .economy_coalesce_window = 0.4F});
  store.push(hit_on(1, 10));
  store.push(hit_on(2, 10));
  store.push(resource_on(3, 0, 8));
  store.push(resource_on(4, 0, 8));

  EXPECT_EQ(store.pending().size(), 4U)
      << "each kind carries its own budget, so a busy economy cannot starve combat";
}

TEST(WorldFeedbackStoreTest, DepositsOnOneDepotMergeButDifferentResourcesDoNot) {
  WorldFeedbackStore store;
  store.push(resource_on(5, 2, 8));
  store.push(resource_on(5, 2, 6));
  store.push(resource_on(5, 3, 4));

  const auto pending = store.pending();
  ASSERT_EQ(pending.size(), 2U);
  EXPECT_EQ(pending[0].amount, 14);
  EXPECT_EQ(pending[0].hits, 2);
  EXPECT_EQ(pending[1].amount, 4);
}

TEST(WorldFeedbackStoreTest, SpendAndGainOnOneAnchorStayApart) {
  WorldFeedbackStore store;
  store.push(resource_on(5, 0, -40));
  store.push(resource_on(5, 0, 10));

  const auto pending = store.pending();
  ASSERT_EQ(pending.size(), 2U)
      << "a refund must not silently cancel a spend into a single tick";
}

TEST(WorldFeedbackStoreTest, EconomyTicksWaitLongerThanCombatTicks) {
  WorldFeedbackStore store;
  store.push(hit_on(1, 10));
  store.push(resource_on(2, 0, 5));

  store.update(0.2F);
  auto ready = store.pop_ready();
  ASSERT_EQ(ready.size(), 1U);
  EXPECT_EQ(ready.front().kind, FeedbackKind::Damage);

  store.update(0.4F);
  ready = store.pop_ready();
  ASSERT_EQ(ready.size(), 1U);
  EXPECT_EQ(ready.front().kind, FeedbackKind::Resource);
}

TEST(WorldFeedbackStoreTest, TradeVariantCarriesBothSides) {
  auto trade = resource_on(6, 0, -40);
  trade.paired_resource = 2;
  trade.paired_amount = 10;
  const auto map = WorldFeedbackStore::to_variant({trade}).front().toMap();
  EXPECT_EQ(map.value("amount").toInt(), -40);
  EXPECT_EQ(map.value("resource").toInt(), 0);
  EXPECT_EQ(map.value("pairedResource").toInt(), 2);
  EXPECT_EQ(map.value("pairedAmount").toInt(), 10);
  EXPECT_EQ(map.value("kind").toString(), QStringLiteral("resource"));
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
