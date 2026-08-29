#include <gtest/gtest.h>
#include <vector>

#include "core/component.h"
#include "core/event_manager.h"
#include "core/world.h"
#include "game/systems/economy_feedback.h"
#include "game/systems/marketplace_system.h"
#include "game/systems/player_resource_registry.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::EconomyFeedbackEvent;
using Engine::Core::EconomyFeedbackKind;
using Game::Systems::ResourceType;

class EconomyFeedbackTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::PlayerResourceRegistry::instance().clear();
    m_subscription = Engine::Core::ScopedEventSubscription<EconomyFeedbackEvent>(
        [this](const EconomyFeedbackEvent& event) { m_events.push_back(event); });
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

  std::vector<EconomyFeedbackEvent> m_events;
  Engine::Core::ScopedEventSubscription<EconomyFeedbackEvent> m_subscription;
};

TEST_F(EconomyFeedbackTest, BuyingAtTheMarketReportsBothSidesAnchoredToTheBuilding) {
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
  EXPECT_EQ(event.kind, EconomyFeedbackKind::Resource);
  EXPECT_EQ(event.resource, static_cast<int>(ResourceType::Gold));
  EXPECT_LT(event.amount, 0) << "gold leaves the treasury on a buy";
  EXPECT_EQ(event.paired_resource, static_cast<int>(ResourceType::Wood));
  EXPECT_EQ(event.paired_amount, market.get_rates().trade_quantity);
}

TEST_F(EconomyFeedbackTest, SellingReportsTheGoodsLeavingAndTheGoldArriving) {
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
}

TEST_F(EconomyFeedbackTest, ARefusedTradeSaysNothing) {
  Engine::Core::World world;
  add_marketplace(world, 1);

  Game::Systems::MarketplaceSystem market;
  EXPECT_FALSE(market.buy_resource(world, 1, ResourceType::Wood))
      << "no gold, no trade";
  EXPECT_TRUE(m_events.empty())
      << "a rejected order must not float a number the player never earned";
}

TEST_F(EconomyFeedbackTest, TradeWithNoMarketplaceEntityStillReportsAnOwner) {
  Engine::Core::World world;
  Game::Systems::publish_trade_feedback(
      2, Engine::Core::NULL_ENTITY, ResourceType::Gold, 40, ResourceType::Iron, 10);

  ASSERT_EQ(m_events.size(), 1U);
  EXPECT_EQ(m_events.front().owner_id, 2);
  EXPECT_EQ(m_events.front().amount, -40);
}

TEST_F(EconomyFeedbackTest, BundlesFanOutOneTickPerResourceAndSkipEmptyOnes) {
  Game::Systems::ResourceAmounts cost;
  cost.set(ResourceType::Wood, 30);
  cost.set(ResourceType::Stone, 12);

  Game::Systems::publish_resource_bundle(1, 77, cost, -1);

  ASSERT_EQ(m_events.size(), 2U);
  for (const auto& event : m_events) {
    EXPECT_EQ(event.anchor_id, 77U);
    EXPECT_LT(event.amount, 0);
  }
  EXPECT_EQ(m_events[0].amount, -30);
  EXPECT_EQ(m_events[1].amount, -12);
}

TEST_F(EconomyFeedbackTest, PositionedBundlesCarryTheWorldPointInsteadOfAnAnchor) {
  Game::Systems::ResourceAmounts cost;
  cost.set(ResourceType::Gold, 25);

  Game::Systems::publish_resource_bundle_at(1, 3.0F, 0.5F, 9.0F, cost, -1);

  ASSERT_EQ(m_events.size(), 1U);
  EXPECT_EQ(m_events.front().anchor_id, Engine::Core::NULL_ENTITY);
  EXPECT_TRUE(m_events.front().has_position);
  EXPECT_FLOAT_EQ(m_events.front().x, 3.0F);
  EXPECT_FLOAT_EQ(m_events.front().z, 9.0F);
}

TEST_F(EconomyFeedbackTest, PopulationChangesAreTheirOwnKind) {
  Game::Systems::publish_population_feedback(1, 42, -18);

  ASSERT_EQ(m_events.size(), 1U);
  EXPECT_EQ(m_events.front().kind, EconomyFeedbackKind::Reserve);
  EXPECT_EQ(m_events.front().amount, -18);
  EXPECT_EQ(m_events.front().resource, -1)
      << "population is not a resource; it must not pick up a resource glyph";
}

TEST_F(EconomyFeedbackTest, NeutralAndZeroChangesAreNotWorthATick) {
  Game::Systems::publish_resource_feedback(0, 5, ResourceType::Gold, 10);
  Game::Systems::publish_resource_feedback(1, 5, ResourceType::Gold, 0);
  Game::Systems::publish_population_feedback(1, 5, 0);

  EXPECT_TRUE(m_events.empty());
}

} // namespace
