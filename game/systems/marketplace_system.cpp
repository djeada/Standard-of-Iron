#include "marketplace_system.h"

#include <vector>

#include "../core/ambient_session.h"
#include "../core/component.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../units/spawn_type.h"
#include "player_resource_registry.h"

namespace Game::Systems {

auto MarketplaceSystem::instance() -> MarketplaceSystem& {
  return *Game::Session::ambient_services().marketplace;
}

auto MarketplaceSystem::get_rates() const -> const MarketplaceTradeRates& {
  return m_rates;
}

auto MarketplaceSystem::owner_has_marketplace(const Engine::Core::World& world,
                                              int owner_id) -> bool {
  std::vector<Engine::Core::Entity*> buildings;
  world.resolve_entities_into(world.entities_with<Engine::Core::BuildingComponent>(),
                              buildings);
  for (const auto* building : buildings) {
    const auto* unit = building->get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr && unit->owner_id == owner_id &&
        unit->spawn_type == Game::Units::SpawnType::Marketplace && unit->health > 0 &&
        !building->has_component<Engine::Core::PendingRemovalComponent>()) {
      return true;
    }
  }
  return false;
}

auto MarketplaceSystem::can_buy(const Engine::Core::World& world,
                                int owner_id,
                                ResourceType resource) const -> bool {
  if (!owner_has_marketplace(world, owner_id)) {
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

auto MarketplaceSystem::can_sell(const Engine::Core::World& world,
                                 int owner_id,
                                 ResourceType resource) const -> bool {
  if (!owner_has_marketplace(world, owner_id)) {
    return false;
  }
  if (resource == ResourceType::Gold || resource == ResourceType::Count) {
    return false;
  }
  return PlayerResourceRegistry::instance().get(owner_id, resource) >=
         m_rates.trade_quantity;
}

auto MarketplaceSystem::buy_resource(const Engine::Core::World& world,
                                     int owner_id,
                                     ResourceType resource) -> bool {
  if (!can_buy(world, owner_id, resource)) {
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

auto MarketplaceSystem::sell_resource(const Engine::Core::World& world,
                                      int owner_id,
                                      ResourceType resource) -> bool {
  if (!can_sell(world, owner_id, resource)) {
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
