#include "mission_wave_tracker.h"

#include <algorithm>
#include <map>
#include <set>

#include "game/core/component.h"
#include "game/core/world.h"

namespace App::Core {

namespace {

auto is_entity_alive(Engine::Core::World& world,
                     Engine::Core::EntityID entity_id) -> bool {
  auto* entity = world.get_entity(entity_id);
  if (entity == nullptr) {
    return false;
  }
  auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  return unit != nullptr && unit->health > 0;
}

} // namespace

auto MissionWaveTracker::total_wave_count() const -> int {
  if (m_waves == nullptr) {
    return 0;
  }
  std::set<int> phases;
  for (const auto& wave : *m_waves) {
    phases.insert(wave.phase_index);
  }
  return static_cast<int>(phases.size());
}

auto MissionWaveTracker::cleared_wave_count() const -> int {
  if (m_waves == nullptr || m_world == nullptr) {
    return 0;
  }

  std::map<int, bool> phase_cleared;
  for (const auto& wave : *m_waves) {
    const bool any_alive = std::any_of(wave.spawned_entity_ids.begin(),
                                       wave.spawned_entity_ids.end(),
                                       [this](Engine::Core::EntityID entity_id) {
                                         return is_entity_alive(*m_world, entity_id);
                                       });
    const bool wave_cleared = wave.spawned && !any_alive;

    auto [it, inserted] = phase_cleared.try_emplace(wave.phase_index, wave_cleared);
    if (!inserted) {
      it->second = it->second && wave_cleared;
    }
  }

  return static_cast<int>(
      std::count_if(phase_cleared.begin(), phase_cleared.end(), [](const auto& entry) {
        return entry.second;
      }));
}

} // namespace App::Core
