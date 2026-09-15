#include "troop_count_registry.h"

#include <algorithm>
#include <cmath>

#include "../core/ambient_session.h"
#include "../core/component_core.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../units/squad.h"
#include "troop_profile_service.h"
#include "units/spawn_type.h"

namespace Game::Systems {

auto troop_type_men(NationID nation_id, Game::Units::SpawnType spawn_type) -> int {
  const auto troop_type = Game::Units::spawn_typeToTroopType(spawn_type);
  if (!troop_type.has_value()) {
    return 0;
  }
  const auto& profile =
      TroopProfileService::instance().get_profile_ref(nation_id, *troop_type);
  return std::max(1, profile.individuals_per_unit);
}

auto squad_men(const Engine::Core::UnitComponent& unit) -> int {
  if (!Game::Units::is_troop_spawn(unit.spawn_type)) {
    return 0;
  }
  const int full = troop_type_men(unit.nation_id, unit.spawn_type);
  if (full <= 1 || Game::Units::squad_is_at_full_strength(unit)) {
    return full;
  }
  return std::clamp(static_cast<int>(std::lround(static_cast<float>(full) *
                                                 Game::Units::squad_fraction(unit))),
                    1,
                    full);
}

auto TroopCountRegistry::instance() -> TroopCountRegistry& {
  return *Game::Session::ambient_services().troop_counts;
}

void TroopCountRegistry::clear() {
  m_troop_counts.clear();
  m_valid = false;
}

auto TroopCountRegistry::get_troop_count(int owner_id) const -> int {
  auto it = m_troop_counts.find(owner_id);
  if (it != m_troop_counts.end()) {
    return it->second;
  }
  return 0;
}

void TroopCountRegistry::rebuild_from_world(const Engine::Core::World& world) {
  m_troop_counts.clear();

  world.for_each_entity([this, &world](const Engine::Core::Entity& entity) {
    const auto* unit = world.try_get<Engine::Core::UnitComponent>(entity.get_id());
    if (unit == nullptr || unit->health <= 0) {
      return;
    }
    if (!Game::Units::is_troop_spawn(unit->spawn_type)) {
      return;
    }
    m_troop_counts[unit->owner_id] += squad_men(*unit);
  });

  m_source_instance_id = world.instance_id();
  m_source_tick_id = world.tick_id();
  m_source_entity_count = world.entity_count();
  m_valid = true;
}

void TroopCountRegistry::refresh_from_world(const Engine::Core::World& world) {
  if (m_valid && m_source_instance_id == world.instance_id() &&
      m_source_tick_id == world.tick_id() &&
      m_source_entity_count == world.entity_count()) {
    return;
  }
  rebuild_from_world(world);
}

} // namespace Game::Systems
