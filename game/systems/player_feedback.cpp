#include "player_feedback.h"

#include <algorithm>

#include "../core/component_structures.h"
#include "../core/event_manager.h"
#include "player_resource_registry.h"

namespace Game::Systems {

namespace {

void announce(const Engine::Core::WorldFeedbackEvent& event) {
  if (event.owner_id <= 0 || event.amount == 0) {
    return;
  }
  Engine::Core::EventManager::instance().publish(event);
}

void announce_resource(int owner_id,
                       Engine::Core::EntityID anchor,
                       ResourceType type,
                       int amount) {
  announce(
      Engine::Core::WorldFeedbackEvent::make_resource(owner_id, anchor, type, amount));
}

void announce_resource_at(
    int owner_id, float x, float y, float z, ResourceType type, int amount) {
  auto event = Engine::Core::WorldFeedbackEvent::make_resource(
      owner_id, Engine::Core::NULL_ENTITY, type, amount);
  event.at(x, y, z);
  announce(event);
}

void move_stock(int owner_id, ResourceType type, int amount) {
  if (owner_id <= 0 || amount == 0) {
    return;
  }
  PlayerResourceRegistry::instance().add(owner_id, type, amount);
}

} // namespace

void grant_resource(int owner_id,
                    Engine::Core::EntityID anchor,
                    ResourceType type,
                    int amount) {
  if (amount <= 0) {
    return;
  }
  move_stock(owner_id, type, amount);
  announce_resource(owner_id, anchor, type, amount);
}

void grant_harvested_resource(int owner_id,
                              Engine::Core::EntityID anchor,
                              ResourceType type,
                              int amount) {
  if (owner_id <= 0 || amount <= 0) {
    return;
  }
  PlayerResourceRegistry::instance().add_harvested(owner_id, type, amount);
  announce_resource(owner_id, anchor, type, amount);
}

void grant_resources(int owner_id,
                     Engine::Core::EntityID anchor,
                     const ResourceAmounts& amounts) {
  for (ResourceType const type : k_all_resource_types) {
    grant_resource(owner_id, anchor, type, amounts.get(type));
  }
}

void grant_resources_at(
    int owner_id, float x, float y, float z, const ResourceAmounts& amounts) {
  for (ResourceType const type : k_all_resource_types) {
    int const amount = amounts.get(type);
    if (amount <= 0) {
      continue;
    }
    move_stock(owner_id, type, amount);
    announce_resource_at(owner_id, x, y, z, type, amount);
  }
}

void spend_resources(int owner_id,
                     Engine::Core::EntityID anchor,
                     const ResourceAmounts& cost) {
  for (ResourceType const type : k_all_resource_types) {
    int const amount = cost.get(type);
    if (amount <= 0) {
      continue;
    }
    move_stock(owner_id, type, -amount);
    announce_resource(owner_id, anchor, type, -amount);
  }
}

void spend_resources_at(
    int owner_id, float x, float y, float z, const ResourceAmounts& cost) {
  for (ResourceType const type : k_all_resource_types) {
    int const amount = cost.get(type);
    if (amount <= 0) {
      continue;
    }
    move_stock(owner_id, type, -amount);
    announce_resource_at(owner_id, x, y, z, type, -amount);
  }
}

void trade_resources(int owner_id,
                     Engine::Core::EntityID anchor,
                     ResourceType spent_type,
                     int spent_amount,
                     ResourceType gained_type,
                     int gained_amount) {
  int const spent = std::abs(spent_amount);
  int const gained = std::abs(gained_amount);
  if (owner_id <= 0 || (spent == 0 && gained == 0)) {
    return;
  }
  move_stock(owner_id, spent_type, -spent);
  move_stock(owner_id, gained_type, gained);
  announce(Engine::Core::WorldFeedbackEvent::make_trade(
      owner_id, anchor, spent_type, spent, gained_type, gained));
}

auto grant_manpower(int owner_id,
                    Engine::Core::EntityID anchor,
                    Engine::Core::ProductionComponent& production,
                    int amount,
                    int ceiling) -> int {
  int const room = std::max(0, ceiling - production.manpower_available);
  int const granted = std::min(std::max(0, amount), room);
  if (granted == 0) {
    return 0;
  }
  production.manpower_available += granted;
  announce(Engine::Core::WorldFeedbackEvent::make_reserve(owner_id, anchor, granted));
  return granted;
}

auto spend_manpower(int owner_id,
                    Engine::Core::EntityID anchor,
                    Engine::Core::ProductionComponent& production,
                    int amount) -> int {
  int const spent = std::max(0, amount);
  if (spent == 0) {
    return 0;
  }
  production.manpower_available -= spent;
  announce(Engine::Core::WorldFeedbackEvent::make_reserve(owner_id, anchor, -spent));
  return spent;
}

auto restore_health(int owner_id,
                    Engine::Core::EntityID target,
                    Engine::Core::UnitComponent& unit,
                    int amount,
                    int ceiling) -> int {
  int const room = std::max(0, ceiling - unit.health);
  int const restored = std::min(std::max(0, amount), room);
  if (restored == 0) {
    return 0;
  }
  unit.health += restored;

  float const severity = unit.max_health > 0
                             ? std::clamp(static_cast<float>(restored) /
                                              static_cast<float>(unit.max_health),
                                          0.0F,
                                          1.5F)
                             : 0.0F;
  announce(Engine::Core::WorldFeedbackEvent::make_heal(
      owner_id, target, restored, severity));
  return restored;
}

} // namespace Game::Systems
