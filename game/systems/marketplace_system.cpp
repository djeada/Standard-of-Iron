#include "marketplace_system.h"

#include "player_resource_registry.h"

namespace Game::Systems {

auto MarketplaceSystem::instance() -> MarketplaceSystem& {
  static MarketplaceSystem s_instance;
  return s_instance;
}

auto MarketplaceSystem::get_rates() const -> const MarketplaceTradeRates& {
  return m_rates;
}

auto MarketplaceSystem::owner_has_marketplace(int owner_id) const -> bool {
  auto it = m_marketplace_count.find(owner_id);
  return it != m_marketplace_count.end() && it->second > 0;
}

void MarketplaceSystem::register_marketplace(int owner_id) {
  m_marketplace_count[owner_id]++;
}

void MarketplaceSystem::unregister_marketplace(int owner_id) {
  auto it = m_marketplace_count.find(owner_id);
  if (it != m_marketplace_count.end() && it->second > 0) {
    it->second--;
  }
}

void MarketplaceSystem::clear() {
  m_marketplace_count.clear();
}

auto MarketplaceSystem::can_buy(int owner_id, ResourceType resource) const -> bool {
  if (!owner_has_marketplace(owner_id)) {
    return false;
  }
  if (resource == ResourceType::Gold || resource == ResourceType::Count) {
    return false;
  }
  int price = 0;
  switch (resource) {
  case ResourceType::Food:
    price = m_rates.buy_price_food;
    break;
  case ResourceType::Wood:
    price = m_rates.buy_price_wood;
    break;
  case ResourceType::Stone:
    price = m_rates.buy_price_stone;
    break;
  case ResourceType::Iron:
    price = m_rates.buy_price_iron;
    break;
  default:
    return false;
  }
  return PlayerResourceRegistry::instance().get(owner_id, ResourceType::Gold) >= price;
}

auto MarketplaceSystem::can_sell(int owner_id, ResourceType resource) const -> bool {
  if (!owner_has_marketplace(owner_id)) {
    return false;
  }
  if (resource == ResourceType::Gold || resource == ResourceType::Count) {
    return false;
  }
  return PlayerResourceRegistry::instance().get(owner_id, resource) >=
         m_rates.trade_quantity;
}

auto MarketplaceSystem::buy_resource(int owner_id, ResourceType resource) -> bool {
  if (!can_buy(owner_id, resource)) {
    return false;
  }
  int price = 0;
  switch (resource) {
  case ResourceType::Food:
    price = m_rates.buy_price_food;
    break;
  case ResourceType::Wood:
    price = m_rates.buy_price_wood;
    break;
  case ResourceType::Stone:
    price = m_rates.buy_price_stone;
    break;
  case ResourceType::Iron:
    price = m_rates.buy_price_iron;
    break;
  default:
    return false;
  }
  PlayerResourceRegistry::instance().add(owner_id, ResourceType::Gold, -price);
  PlayerResourceRegistry::instance().add(owner_id, resource, m_rates.trade_quantity);
  return true;
}

auto MarketplaceSystem::sell_resource(int owner_id, ResourceType resource) -> bool {
  if (!can_sell(owner_id, resource)) {
    return false;
  }
  int sell_price = 0;
  switch (resource) {
  case ResourceType::Food:
    sell_price = m_rates.sell_price_food;
    break;
  case ResourceType::Wood:
    sell_price = m_rates.sell_price_wood;
    break;
  case ResourceType::Stone:
    sell_price = m_rates.sell_price_stone;
    break;
  case ResourceType::Iron:
    sell_price = m_rates.sell_price_iron;
    break;
  default:
    return false;
  }
  PlayerResourceRegistry::instance().add(owner_id, resource, -m_rates.trade_quantity);
  PlayerResourceRegistry::instance().add(owner_id, ResourceType::Gold, sell_price);
  return true;
}

} // namespace Game::Systems
