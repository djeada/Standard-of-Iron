#pragma once

#include <string_view>
#include <unordered_map>

#include "resource_types.h"

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
  static auto instance() -> MarketplaceSystem&;

  [[nodiscard]] auto get_rates() const -> const MarketplaceTradeRates&;

  [[nodiscard]] auto can_buy(int owner_id, ResourceType resource) const -> bool;
  [[nodiscard]] auto can_sell(int owner_id, ResourceType resource) const -> bool;
  [[nodiscard]] auto owner_has_marketplace(int owner_id) const -> bool;

  void register_marketplace(int owner_id);
  void unregister_marketplace(int owner_id);

  auto buy_resource(int owner_id, ResourceType resource) -> bool;
  auto sell_resource(int owner_id, ResourceType resource) -> bool;

private:
  MarketplaceTradeRates m_rates;
  std::unordered_map<int, int> m_marketplace_count;
};

} // namespace Game::Systems
