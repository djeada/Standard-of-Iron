#include <gtest/gtest.h>
#include <vector>

#include "core/component_structures.h"
#include "core/event_manager.h"
#include "core/world.h"
#include "game/systems/marketplace_system.h"
#include "game/systems/player_feedback.h"
#include "game/systems/player_resource_registry.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::WorldFeedbackEvent;
using Engine::Core::WorldFeedbackKind;
using Game::Systems::ResourceType;

class PlayerFeedbackTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::PlayerResourceRegistry::instance().clear();
    m_subscription = Engine::Core::ScopedEventSubscription<WorldFeedbackEvent>(
        [this](const WorldFeedbackEvent& event) { m_events.push_back(event); });
  }

  void TearDown() override {
    m_subscription = {};
    m_events.clear();
    Game::Systems::PlayerResourceRegistry::instance().clear();
  }

  static auto add_marketplace(Engine::Core::World& world,
                              int owner_id) -> Engine::Core::EntityID {
    auto* entity = world.create_entity();
    entity->add_component<Engine::Core::TransformComponent>(4.0F, 0.0F, 6.0F);
    entity->add_component<Engine::Core::BuildingComponent>();
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->owner_id = owner_id;
    unit->health = 500;
    unit->max_health = 500;
    unit->spawn_type = Game::Units::SpawnType::Marketplace;
    return entity->get_id();
  }

  static auto stock(int owner_id, ResourceType type) -> int {
    return Game::Systems::PlayerResourceRegistry::instance().get(owner_id, type);
  }

  std::vector<WorldFeedbackEvent> m_events;
  Engine::Core::ScopedEventSubscription<WorldFeedbackEvent> m_subscription;
};

TEST_F(PlayerFeedbackTest, BuyingAtTheMarketReportsBothSidesAnchoredToTheBuilding) {
  Engine::Core::World world;
  const auto marketplace = add_marketplace(world, 1);
  Game::Systems::PlayerResourceRegistry::instance().add(1, ResourceType::Gold, 500);

  Game::Systems::MarketplaceSystem market;
  ASSERT_TRUE(market.buy_resource(world, 1, ResourceType::Wood));

  ASSERT_EQ(m_events.size(), 1U);
  const auto& event = m_events.front();
  EXPECT_EQ(event.owner_id, 1);
  EXPECT_EQ(event.anchor_id, marketplace)
      << "the trade floats over the marketplace it happened at";
  EXPECT_EQ(event.kind, WorldFeedbackKind::Resource);
  EXPECT_EQ(event.resource, static_cast<int>(ResourceType::Gold));
  EXPECT_LT(event.amount, 0) << "gold leaves the treasury on a buy";
  EXPECT_EQ(event.paired_resource, static_cast<int>(ResourceType::Wood));
  EXPECT_EQ(event.paired_amount, market.get_rates().trade_quantity);

  EXPECT_EQ(stock(1, ResourceType::Wood), market.get_rates().trade_quantity)
      << "the same call has to move the stock it announced";
  EXPECT_LT(stock(1, ResourceType::Gold), 500);
}

TEST_F(PlayerFeedbackTest, SellingReportsTheGoodsLeavingAndTheGoldArriving) {
  Engine::Core::World world;
  add_marketplace(world, 1);
  Game::Systems::PlayerResourceRegistry::instance().add(1, ResourceType::Stone, 200);

  Game::Systems::MarketplaceSystem market;
  ASSERT_TRUE(market.sell_resource(world, 1, ResourceType::Stone));

  ASSERT_EQ(m_events.size(), 1U);
  const auto& event = m_events.front();
  EXPECT_EQ(event.resource, static_cast<int>(ResourceType::Stone));
  EXPECT_EQ(event.amount, -market.get_rates().trade_quantity);
  EXPECT_EQ(event.paired_resource, static_cast<int>(ResourceType::Gold));
  EXPECT_GT(event.paired_amount, 0);
  EXPECT_EQ(stock(1, ResourceType::Gold), event.paired_amount);
}

TEST_F(PlayerFeedbackTest, ARefusedTradeSaysNothing) {
  Engine::Core::World world;
  add_marketplace(world, 1);

  Game::Systems::MarketplaceSystem market;
  EXPECT_FALSE(market.buy_resource(world, 1, ResourceType::Wood))
      << "no gold, no trade";
  EXPECT_TRUE(m_events.empty())
      << "a rejected order must not float a number the player never earned";
}

TEST_F(PlayerFeedbackTest, TradeWithNoMarketplaceEntityStillReportsAnOwner) {
  Game::Systems::trade_resources(
      2, Engine::Core::NULL_ENTITY, ResourceType::Gold, 40, ResourceType::Iron, 10);

  ASSERT_EQ(m_events.size(), 1U);
  EXPECT_EQ(m_events.front().owner_id, 2);
  EXPECT_EQ(m_events.front().amount, -40);
}

TEST_F(PlayerFeedbackTest, SpendingFansOutOneTickPerResourceAndSkipsEmptyOnes) {
  Game::Systems::ResourceAmounts cost;
  cost.set(ResourceType::Wood, 30);
  cost.set(ResourceType::Stone, 12);
  Game::Systems::PlayerResourceRegistry::instance().add(1, ResourceType::Wood, 100);
  Game::Systems::PlayerResourceRegistry::instance().add(1, ResourceType::Stone, 100);

  Game::Systems::spend_resources(1, 77, cost);

  ASSERT_EQ(m_events.size(), 2U);
  for (const auto& event : m_events) {
    EXPECT_EQ(event.anchor_id, 77U);
    EXPECT_LT(event.amount, 0);
  }
  EXPECT_EQ(m_events[0].amount, -30);
  EXPECT_EQ(m_events[1].amount, -12);
  EXPECT_EQ(stock(1, ResourceType::Wood), 70);
  EXPECT_EQ(stock(1, ResourceType::Stone), 88);
}

TEST_F(PlayerFeedbackTest, PositionedChangesCarryTheWorldPointInsteadOfAnAnchor) {
  Game::Systems::ResourceAmounts cost;
  cost.set(ResourceType::Gold, 25);
  Game::Systems::PlayerResourceRegistry::instance().add(1, ResourceType::Gold, 100);

  Game::Systems::spend_resources_at(1, 3.0F, 0.5F, 9.0F, cost);

  ASSERT_EQ(m_events.size(), 1U);
  EXPECT_EQ(m_events.front().anchor_id, Engine::Core::NULL_ENTITY);
  EXPECT_TRUE(m_events.front().has_position);
  EXPECT_FLOAT_EQ(m_events.front().x, 3.0F);
  EXPECT_FLOAT_EQ(m_events.front().z, 9.0F);
  EXPECT_EQ(stock(1, ResourceType::Gold), 75);
}

TEST_F(PlayerFeedbackTest, HarvestedLoadsCountTowardsWhatWasEverGathered) {
  Game::Systems::grant_harvested_resource(1, 9, ResourceType::Wood, 40);

  ASSERT_EQ(m_events.size(), 1U);
  EXPECT_EQ(m_events.front().amount, 40);
  EXPECT_EQ(stock(1, ResourceType::Wood), 40);
  EXPECT_EQ(Game::Systems::PlayerResourceRegistry::instance().get_harvested_all(1).get(
                ResourceType::Wood),
            40);
}

TEST_F(PlayerFeedbackTest, ManpowerChangesAreTheirOwnKind) {
  Engine::Core::ProductionComponent production;
  production.manpower_available = 40;

  EXPECT_EQ(Game::Systems::spend_manpower(1, 42, production, 18), 18);

  ASSERT_EQ(m_events.size(), 1U);
  EXPECT_EQ(m_events.front().kind, WorldFeedbackKind::Reserve);
  EXPECT_EQ(m_events.front().amount, -18);
  EXPECT_EQ(m_events.front().resource, -1)
      << "manpower is not a resource; it must not pick up a resource glyph";
  EXPECT_EQ(production.manpower_available, 22);
}

TEST_F(PlayerFeedbackTest, AManpowerGrantStopsAtTheCeilingAndOnlyReportsWhatLanded) {
  Engine::Core::ProductionComponent production;
  production.manpower_available = 95;

  EXPECT_EQ(Game::Systems::grant_manpower(1, 42, production, 20, 100), 5);

  ASSERT_EQ(m_events.size(), 1U);
  EXPECT_EQ(m_events.front().amount, 5)
      << "a full barracks must not flash a number for men it turned away";
  EXPECT_EQ(production.manpower_available, 100);

  m_events.clear();
  EXPECT_EQ(Game::Systems::grant_manpower(1, 42, production, 20, 100), 0);
  EXPECT_TRUE(m_events.empty());
}

TEST_F(PlayerFeedbackTest, HealthPutBackIsAnnouncedAsWhatActuallyLanded) {
  Engine::Core::UnitComponent unit;
  unit.health = 90;
  unit.max_health = 100;

  EXPECT_EQ(Game::Systems::restore_health(1, 7, unit, 25, unit.max_health), 10);

  ASSERT_EQ(m_events.size(), 1U);
  const auto& event = m_events.front();
  EXPECT_EQ(event.kind, WorldFeedbackKind::Heal);
  EXPECT_EQ(event.anchor_id, 7U);
  EXPECT_EQ(event.amount, 10) << "a heal into a nearly full bar is a small number";
  EXPECT_GT(event.severity, 0.0F);
  EXPECT_EQ(unit.health, 100);
}

TEST_F(PlayerFeedbackTest, HealingAFullBarSaysNothing) {
  Engine::Core::UnitComponent unit;
  unit.health = 100;
  unit.max_health = 100;

  EXPECT_EQ(Game::Systems::restore_health(1, 7, unit, 25, unit.max_health), 0);
  EXPECT_TRUE(m_events.empty());
  EXPECT_EQ(unit.health, 100);
}

TEST_F(PlayerFeedbackTest, AMauledSquadIsOnlyHealedBackToItsOwnCeiling) {
  Engine::Core::UnitComponent unit;
  unit.health = 40;
  unit.max_health = 100;

  EXPECT_EQ(Game::Systems::restore_health(1, 7, unit, 30, 60), 20)
      << "the ceiling is the caller's, and the number shown has to respect it";
  EXPECT_EQ(unit.health, 60);
}

TEST_F(PlayerFeedbackTest, NeutralAndZeroChangesAreNotWorthATick) {
  Game::Systems::grant_resource(0, 5, ResourceType::Gold, 10);
  Game::Systems::grant_resource(1, 5, ResourceType::Gold, 0);

  Engine::Core::ProductionComponent production;
  Game::Systems::spend_manpower(1, 5, production, 0);

  EXPECT_TRUE(m_events.empty());
  EXPECT_EQ(stock(0, ResourceType::Gold), 0);
}

} // namespace
