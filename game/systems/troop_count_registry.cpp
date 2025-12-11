#include "troop_count_registry.h"
#include "../core/component.h"
#include "../core/world.h"
#include "../units/troop_config.h"
#include "core/event_manager.h"
#include "units/spawn_type.h"

namespace Game::Systems {

auto TroopCountRegistry::instance() -> TroopCountRegistry & {
  static TroopCountRegistry inst;
  return inst;
}

void TroopCountRegistry::initialize() {
  m_unit_spawned_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>(
          [this](const Engine::Core::UnitSpawnedEvent &e) {
            on_unit_spawned(e);
          });

  m_unit_died_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>(
          [this](const Engine::Core::UnitDiedEvent &e) { on_unit_died(e); });
}

void TroopCountRegistry::clear() { m_troop_counts.clear(); }

auto TroopCountRegistry::getTroopCount(int owner_id) const -> int {
  auto it = m_troop_counts.find(owner_id);
  if (it != m_troop_counts.end()) {
    return it->second;
  }
  return 0;
}

void TroopCountRegistry::on_unit_spawned(
    const Engine::Core::UnitSpawnedEvent &event) {
  if (event.spawn_type == Game::Units::SpawnType::Barracks) {
    return;
  }

  int const individuals_per_unit =
      Game::Units::TroopConfig::instance().getIndividualsPerUnit(
          event.spawn_type);
  m_troop_counts[event.owner_id] += individuals_per_unit;
}

void TroopCountRegistry::on_unit_died(
    const Engine::Core::UnitDiedEvent &event) {
  if (event.spawn_type == Game::Units::SpawnType::Barracks) {
    return;
  }

  int const individuals_per_unit =
      Game::Units::TroopConfig::instance().getIndividualsPerUnit(
          event.spawn_type);
  m_troop_counts[event.owner_id] -= individuals_per_unit;
  if (m_troop_counts[event.owner_id] < 0) {
    m_troop_counts[event.owner_id] = 0;
  }
}

void TroopCountRegistry::rebuild_from_world(Engine::Core::World &world) {
  m_troop_counts.clear();

  auto entities = world.get_entities_with<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *unit = e->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->health <= 0) {
      continue;
    }

    if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
      continue;
    }

    int const individuals_per_unit =
        Game::Units::TroopConfig::instance().getIndividualsPerUnit(
            unit->spawn_type);
    m_troop_counts[unit->owner_id] += individuals_per_unit;
  }
}

} // namespace Game::Systems
