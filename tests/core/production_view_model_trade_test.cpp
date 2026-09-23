#include <gtest/gtest.h>
#include <mutex>

#include "app/core/client_context.h"
#include "app/viewmodels/production_view_model.h"
#include "game/command/command_queue.h"
#include "game/core/component_core.h"
#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "game/render_bridge/selection_controller.h"
#include "game/session/selection_service.h"
#include "game/session/session_context.h"
#include "game/systems/marketplace_system.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/resource_types.h"
#include "game/units/spawn_type.h"

namespace {

using Game::Systems::ResourceType;

class StubClientHost : public App::Core::ClientHost {
public:
  void ensure_initialized() override {}
  auto lock_frame() -> std::unique_lock<std::recursive_mutex> override {
    return std::unique_lock<std::recursive_mutex>(m_mutex);
  }
  void set_cursor_mode(CursorMode) override {}

private:
  std::recursive_mutex m_mutex;
};

class ProductionViewModelTradeTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_scope = std::make_unique<Game::Session::ScopedSession>(m_session);
    m_session.owners().register_owner_with_id(
        1, Game::Systems::OwnerType::Player, "player");
    auto& world = m_session.world();
    m_selection_system = &m_session.selection();
    m_selection = std::make_unique<Game::Systems::SelectionController>(
        &world, m_selection_system, nullptr);
    m_context.session = &m_session;
    m_context.world = &world;
    m_context.local_owner_id = 1;
    m_context.selection = m_selection.get();
    m_view_model =
        std::make_unique<App::ViewModels::ProductionViewModel>(m_context, m_host);
  }

  auto spawn_marketplace() -> Engine::Core::EntityID {
    auto* market = m_session.world().create_entity();
    market->add_component<Engine::Core::TransformComponent>();
    market->add_component<Engine::Core::BuildingComponent>();
    auto* unit =
        market->add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
    unit->owner_id = 1;
    unit->spawn_type = Game::Units::SpawnType::Marketplace;
    return market->get_id();
  }

  Game::Session::SessionContext m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
  Game::Session::SelectionService* m_selection_system = nullptr;
  std::unique_ptr<Game::Systems::SelectionController> m_selection;
  App::Core::ClientContext m_context;
  StubClientHost m_host;
  std::unique_ptr<App::ViewModels::ProductionViewModel> m_view_model;
};

TEST_F(ProductionViewModelTradeTest, BuyingFromTheSelectedMarketplaceMovesStock) {
  const auto market = spawn_marketplace();
  auto& economy = m_session.economy();
  economy.add(1, ResourceType::Gold, 200);
  m_selection->select_single_unit(market, 1);
  m_view_model->publish_frame();

  ASSERT_TRUE(m_view_model->has_selected_type(QStringLiteral("marketplace")));
  ASSERT_TRUE(
      m_view_model->selected_marketplace_state().value("has_marketplace").toBool());

  const int food_before = economy.get(1, ResourceType::Food);
  EXPECT_TRUE(m_view_model->marketplace_buy(QStringLiteral("food")));
  m_session.commands().drain(m_session.world(), 1);
  EXPECT_GT(economy.get(1, ResourceType::Food), food_before);
  EXPECT_LT(economy.get(1, ResourceType::Gold), 200);
}

TEST_F(ProductionViewModelTradeTest, ResourcesGoToAnAllyAndARequestIsQueuedForIt) {
  spawn_marketplace();
  m_session.owners().register_owner_with_id(3, Game::Systems::OwnerType::AI, "Hanno");
  m_session.owners().register_owner_with_id(5, Game::Systems::OwnerType::AI, "Rome");
  m_session.owners().set_owner_team(1, 1);
  m_session.owners().set_owner_team(3, 1);
  m_session.owners().set_owner_team(5, 2);
  auto& economy = m_session.economy();
  economy.set(1, ResourceType::Wood, 300);
  economy.set(3, ResourceType::Wood, 0);

  const auto allies = m_view_model->marketplace_allies();
  ASSERT_EQ(allies.size(), 1) << "only team-mates are offered";
  EXPECT_EQ(allies.front().toMap().value("owner_id").toInt(), 3);

  ASSERT_TRUE(m_view_model->send_to_ally(3, QStringLiteral("wood"), 100));
  m_session.commands().drain(m_session.world(), 1);
  EXPECT_EQ(economy.get(1, ResourceType::Wood), 200);
  EXPECT_EQ(economy.get(3, ResourceType::Wood), 100);
  const auto sent = m_session.marketplace().take_ally_answers();
  ASSERT_EQ(sent.size(), 1U);
  EXPECT_EQ(sent.front().granted, 100);

  EXPECT_FALSE(m_view_model->send_to_ally(5, QStringLiteral("wood"), 50))
      << "an enemy is not a trading partner";

  ASSERT_TRUE(m_view_model->request_from_ally(3, QStringLiteral("gold"), 50));
  m_session.commands().drain(m_session.world(), 1);
  const auto asked = m_session.marketplace().take_ally_requests_for(3);
  ASSERT_EQ(asked.size(), 1U);
  EXPECT_EQ(asked.front().requester, 1);
  EXPECT_EQ(asked.front().resource, ResourceType::Gold);
  EXPECT_EQ(asked.front().amount, 50);
}

} // namespace
