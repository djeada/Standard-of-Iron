#include "marketplace_system.h"

#include <algorithm>
#include <vector>

#include "../core/ambient_session.h"
#include "../core/component_gameplay.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../units/spawn_type.h"
#include "player_feedback.h"
#include "player_resource_registry.h"

namespace Game::Systems {

auto MarketplaceSystem::instance() -> MarketplaceSystem& {
  return *Game::Session::ambient_services().marketplace;
}

auto MarketplaceSystem::get_rates() const -> const MarketplaceTradeRates& {
  return m_rates;
}

auto MarketplaceSystem::find_marketplace(const Engine::Core::World& world,
                                         int owner_id) -> Engine::Core::EntityID {
  std::vector<Engine::Core::Entity*> buildings;
  world.resolve_entities_into(world.entities_with<Engine::Core::BuildingComponent>(),
                              buildings);
  for (const auto* building : buildings) {
    const auto* unit = building->get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr && unit->owner_id == owner_id &&
        unit->spawn_type == Game::Units::SpawnType::Marketplace && unit->health > 0 &&
        !building->has_component<Engine::Core::PendingRemovalComponent>() &&
        !building->has_component<Engine::Core::DismantleSiteComponent>()) {
      return building->get_id();
    }
  }
  return Engine::Core::NULL_ENTITY;
}

auto MarketplaceSystem::owner_has_marketplace(const Engine::Core::World& world,
                                              int owner_id) -> bool {
  return find_marketplace(world, owner_id) != Engine::Core::NULL_ENTITY;
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
  trade_resources(owner_id,
                  find_marketplace(world, owner_id),
                  ResourceType::Gold,
                  price,
                  resource,
                  m_rates.trade_quantity);
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
  trade_resources(owner_id,
                  find_marketplace(world, owner_id),
                  resource,
                  m_rates.trade_quantity,
                  ResourceType::Gold,
                  sell_price);
  return true;
}

auto MarketplaceSystem::send_to_ally(const Engine::Core::World& world,
                                     int from_owner,
                                     int to_owner,
                                     ResourceType resource,
                                     int amount) -> int {
  if (from_owner <= 0 || to_owner <= 0 || from_owner == to_owner || amount <= 0) {
    return 0;
  }
  auto& economy = PlayerResourceRegistry::instance();
  const int moved = std::min(
      {amount, k_max_ally_tribute, std::max(0, economy.get(from_owner, resource))});
  if (moved <= 0) {
    return 0;
  }
  ResourceAmounts parcel{};
  parcel.set(resource, moved);
  spend_resources(from_owner, find_marketplace(world, from_owner), parcel);
  grant_resource(to_owner, find_marketplace(world, to_owner), resource, moved);
  return moved;
}

void MarketplaceSystem::queue_ally_request(const AllyTributeRequest& request) {
  constexpr std::size_t k_pending_cap = 32;
  if (m_ally_requests.size() < k_pending_cap) {
    m_ally_requests.push_back(request);
  }
}

auto MarketplaceSystem::take_ally_requests_for(int giver)
    -> std::vector<AllyTributeRequest> {
  std::vector<AllyTributeRequest> taken;
  std::erase_if(m_ally_requests, [&](const AllyTributeRequest& request) {
    if (request.giver != giver) {
      return false;
    }
    taken.push_back(request);
    return true;
  });
  return taken;
}

void MarketplaceSystem::record_ally_answer(const AllyTributeAnswer& answer) {
  constexpr std::size_t k_answer_cap = 32;
  if (m_ally_answers.size() < k_answer_cap) {
    m_ally_answers.push_back(answer);
  }
}

auto MarketplaceSystem::take_ally_answers() -> std::vector<AllyTributeAnswer> {
  std::vector<AllyTributeAnswer> taken;
  taken.swap(m_ally_answers);
  return taken;
}

void MarketplaceSystem::clear_ally_exchange() {
  m_ally_requests.clear();
  m_ally_answers.clear();
}

} // namespace Game::Systems
