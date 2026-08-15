#pragma once

#include "resource_types.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

struct MarketplaceTradeRates {
  int buy_price_food = 10;
  int buy_price_wood = 12;
  int buy_price_stone = 15;
  int buy_price_iron = 20;
  int sell_price_food = 5;
  int sell_price_wood = 6;
  int sell_price_stone = 8;
  int sell_price_iron = 12;
  int trade_quantity = 10;
};

class MarketplaceSystem {
public:
  MarketplaceSystem() = default;
  ~MarketplaceSystem() = default;
  MarketplaceSystem(const MarketplaceSystem&) = delete;
  MarketplaceSystem(MarketplaceSystem&&) = delete;
  auto operator=(const MarketplaceSystem&) -> MarketplaceSystem& = delete;
  auto operator=(MarketplaceSystem&&) -> MarketplaceSystem& = delete;

  static auto instance() -> MarketplaceSystem&;

  [[nodiscard]] auto get_rates() const -> const MarketplaceTradeRates&;

  [[nodiscard]] static auto owner_has_marketplace(const Engine::Core::World& world,
                                                  int owner_id) -> bool;

  [[nodiscard]] auto can_buy(const Engine::Core::World& world,
                             int owner_id,
                             ResourceType resource) const -> bool;
  [[nodiscard]] auto can_sell(const Engine::Core::World& world,
                              int owner_id,
                              ResourceType resource) const -> bool;

  auto buy_resource(const Engine::Core::World& world,
                    int owner_id,
                    ResourceType resource) -> bool;
  auto sell_resource(const Engine::Core::World& world,
                     int owner_id,
                     ResourceType resource) -> bool;

private:
  MarketplaceTradeRates m_rates;
};

} // namespace Game::Systems
