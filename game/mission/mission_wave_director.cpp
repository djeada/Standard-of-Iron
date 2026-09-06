#include "game/mission/mission_wave_director.h"

#include <QDebug>
#include <QJsonArray>
#include <QVariantList>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

#include "game/audio/cue_ids.h"
#include "game/core/component_structures.h"
#include "game/core/world.h"
#include "game/mission/mission_waves.h"

namespace Game::Mission {

namespace {

constexpr float k_wave_survivor_clear_fraction = 0.1F;

auto is_entity_alive(Engine::Core::World& world,
                     Engine::Core::EntityID entity_id) -> bool {
  auto* entity = world.get_entity(entity_id);
  if (entity == nullptr) {
    return false;
  }
  auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  return unit != nullptr && unit->health > 0;
}

auto is_entity_holding_the_field(Engine::Core::World& world,
                                 Engine::Core::EntityID entity_id) -> bool {
  auto* entity = world.get_entity(entity_id);
  if (entity == nullptr) {
    return false;
  }
  auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr || unit->health <= 0) {
    return false;
  }
  const auto* morale = entity->get_component<Engine::Core::MoraleComponent>();
  return morale == nullptr || !morale->routing;
}

} // namespace

void MissionWaveDirector::bind(std::vector<PendingMissionWave>* waves,
                               Engine::Core::World* world) {
  m_waves = waves;
  m_world = world;
  m_settled_phases.clear();
  m_last_status.clear();
  refresh_ready_times();
}

void MissionWaveDirector::reset() {
  m_waves = nullptr;
  m_world = nullptr;
  m_elapsed = 0.0F;
  m_announced_cleared_phases = 0;
  m_announced_all_cleared = false;
  m_settled_phases.clear();
  m_last_status.clear();
}

auto MissionWaveDirector::wave_is_cleared(const PendingMissionWave& wave) const
    -> bool {
  if (!wave.spawned || m_world == nullptr) {
    return false;
  }
  if (wave.spawned_entity_ids.empty()) {
    return true;
  }

  const auto total = static_cast<int>(wave.spawned_entity_ids.size());
  int alive = 0;
  int holding = 0;
  for (const auto entity_id : wave.spawned_entity_ids) {
    if (is_entity_alive(*m_world, entity_id)) {
      alive++;
    }
    if (is_entity_holding_the_field(*m_world, entity_id)) {
      holding++;
    }
  }

  if (holding == 0) {
    return true;
  }

  const int survivor_ceiling = static_cast<int>(
      std::floor(static_cast<float>(total) * k_wave_survivor_clear_fraction));
  return alive <= survivor_ceiling;
}

auto MissionWaveDirector::phase_indices() const -> std::vector<int> {
  std::vector<int> phases;
  if (m_waves == nullptr) {
    return phases;
  }
  std::set<int> unique;
  for (const auto& wave : *m_waves) {
    unique.insert(wave.phase_index);
  }
  phases.assign(unique.begin(), unique.end());
  return phases;
}

auto MissionWaveDirector::is_phase_cleared(int phase_index) const -> bool {
  if (m_waves == nullptr) {
    return false;
  }
  bool any = false;
  for (const auto& wave : *m_waves) {
    if (wave.phase_index != phase_index) {
      continue;
    }
    any = true;
    if (!wave.cleared) {
      return false;
    }
  }
  return any;
}

auto MissionWaveDirector::is_phase_spawned(int phase_index) const -> bool {
  if (m_waves == nullptr) {
    return false;
  }
  bool any = false;
  for (const auto& wave : *m_waves) {
    if (wave.phase_index != phase_index) {
      continue;
    }
    any = true;
    if (!wave.spawned) {
      return false;
    }
  }
  return any;
}

auto MissionWaveDirector::live_units_in_phase(int phase_index) const -> int {
  if (m_waves == nullptr || m_world == nullptr) {
    return 0;
  }
  int live = 0;
  for (const auto& wave : *m_waves) {
    if (wave.phase_index != phase_index || !wave.spawned) {
      continue;
    }
    for (const auto entity_id : wave.spawned_entity_ids) {
      if (is_entity_alive(*m_world, entity_id)) {
        live++;
      }
    }
  }
  return live;
}

auto MissionWaveDirector::ready_time_for(std::size_t index) const -> float {
  const auto& wave = (*m_waves)[index];
  if (wave.trigger == Game::Mission::WaveTriggerMode::Time) {
    return wave.trigger_time;
  }

  float predecessor_cleared_at = 0.0F;
  bool has_predecessor = false;
  for (std::size_t i = index; i-- > 0;) {
    const auto& candidate = (*m_waves)[i];
    if (candidate.ai_id != wave.ai_id) {
      continue;
    }
    has_predecessor = true;
    if (!candidate.cleared) {
      return -1.0F;
    }
    predecessor_cleared_at = candidate.cleared_at;
    break;
  }

  if (!has_predecessor) {
    return wave.trigger_time;
  }
  return predecessor_cleared_at + wave.grace_seconds;
}

void MissionWaveDirector::refresh_ready_times() {
  if (m_waves == nullptr) {
    return;
  }
  for (std::size_t i = 0; i < m_waves->size(); ++i) {
    (*m_waves)[i].ready_at = ready_time_for(i);
  }
}

auto MissionWaveDirector::advance() -> Effects {
  Effects effects;
  if (m_waves == nullptr || m_world == nullptr || m_waves->empty()) {
    return effects;
  }

  for (auto& wave : *m_waves) {
    if (wave.cleared || !wave.spawned) {
      continue;
    }
    if (wave_is_cleared(wave)) {
      wave.cleared = true;
      wave.cleared_at = m_elapsed;
    }
  }

  refresh_ready_times();

  for (std::size_t i = 0; i < m_waves->size(); ++i) {
    auto& wave = (*m_waves)[i];
    if (wave.spawned || wave.ready_at < 0.0F) {
      continue;
    }
    if (!wave.warned && m_elapsed >= wave.ready_at - wave.warning_seconds) {
      wave.warned = true;
      effects.announcements.append(wave_incoming_announcement(wave));
      effects.incoming_waves.push_back({.owner_id = wave.owner_id,
                                        .phase_index = wave.phase_index,
                                        .phase_count = wave.phase_count,
                                        .final_wave = wave.final_wave});
      effects.audio_cues.append(
          QString::fromLatin1(Game::Audio::Cue::k_alert_enemy_reinforcements));
      effects.status_changed = true;
    }
    if (m_elapsed >= wave.ready_at) {
      effects.waves_to_spawn.push_back(i);
    }
  }

  const auto phases = phase_indices();
  int cleared_phases = 0;
  for (const int phase : phases) {
    if (!is_phase_cleared(phase)) {
      continue;
    }
    cleared_phases++;
    if (m_settled_phases.contains(phase)) {
      continue;
    }
    m_settled_phases.insert(phase);

    const PendingMissionWave* representative = nullptr;
    for (const auto& wave : *m_waves) {
      if (wave.phase_index != phase) {
        continue;
      }
      representative = &wave;
      for (const auto type : Game::Systems::k_all_resource_types) {
        effects.reward.add(type, wave.clear_reward.get(type));
      }
    }
    if (representative != nullptr) {
      effects.announcements.append(wave_cleared_announcement(*representative));
      effects.cleared_waves.push_back({.owner_id = representative->owner_id,
                                       .phase_index = representative->phase_index,
                                       .phase_count = representative->phase_count,
                                       .final_wave = representative->final_wave});
      effects.audio_cues.append(
          QString::fromLatin1(Game::Audio::Cue::k_alert_objective_complete));
    }
    effects.status_changed = true;
  }
  m_announced_cleared_phases = cleared_phases;

  const bool all_cleared =
      !phases.empty() && cleared_phases == static_cast<int>(phases.size());
  if (all_cleared && !m_announced_all_cleared) {
    m_announced_all_cleared = true;
    effects.all_cleared = true;
    effects.announcements.append(all_waves_cleared_announcement(m_waves->back()));
    effects.status_changed = true;
  }

  const QVariantMap next_status = status();
  if (next_status != m_last_status) {
    m_last_status = next_status;
    effects.status_changed = true;
  }

  return effects;
}

void MissionWaveDirector::note_spawned(
    std::size_t index, std::vector<Engine::Core::EntityID> spawned_entity_ids) {
  if (m_waves == nullptr || index >= m_waves->size()) {
    return;
  }
  auto& wave = (*m_waves)[index];
  wave.spawned = true;
  wave.warned = true;
  wave.spawned_entity_ids = std::move(spawned_entity_ids);
}

auto MissionWaveDirector::status() const -> QVariantMap {
  QVariantMap map;
  const auto phases = phase_indices();
  map["active"] = !phases.empty();
  map["total"] = static_cast<int>(phases.size());

  if (phases.empty() || m_waves == nullptr) {
    map["cleared"] = 0;
    map["current"] = 0;
    map["seconds_until_next"] = -1.0;
    map["warning"] = false;
    map["live_enemies"] = 0;
    map["state"] = QStringLiteral("idle");
    map["alerts"] = QVariantList{};
    return map;
  }

  int cleared = 0;
  for (const int phase : phases) {
    if (is_phase_cleared(phase)) {
      cleared++;
    }
  }
  map["cleared"] = cleared;

  int engaged_phase = 0;
  for (const int phase : phases) {
    if (is_phase_spawned(phase) && !is_phase_cleared(phase)) {
      engaged_phase = phase;
      break;
    }
  }

  float seconds_until_next = -1.0F;
  int pending_phase = 0;
  QVariantList alerts;
  for (const auto& wave : *m_waves) {
    if (wave.spawned) {
      continue;
    }
    if (wave.ready_at < 0.0F) {
      if (pending_phase == 0) {
        pending_phase = wave.phase_index;
      }
      continue;
    }
    const float remaining = std::max(0.0F, wave.ready_at - m_elapsed);
    if (seconds_until_next < 0.0F || remaining < seconds_until_next) {
      seconds_until_next = remaining;
      pending_phase = wave.phase_index;
    }
    if (wave.warned) {
      for (const auto& entry : wave.entry_positions()) {
        QVariantMap alert;
        alert["x"] = entry.x();
        alert["z"] = entry.z();
        alert["phase"] = wave.phase_index;
        alerts.append(alert);
      }
    }
  }

  const bool warning =
      seconds_until_next >= 0.0F &&
      std::any_of(m_waves->begin(), m_waves->end(), [](const PendingMissionWave& wave) {
        return wave.warned && !wave.spawned;
      });

  const int current_phase = engaged_phase > 0
                                ? engaged_phase
                                : (pending_phase > 0 ? pending_phase : phases.back());

  QString state;
  if (cleared == static_cast<int>(phases.size())) {
    state = QStringLiteral("complete");
  } else if (engaged_phase > 0) {
    state = QStringLiteral("engaged");
  } else if (warning) {
    state = QStringLiteral("incoming");
  } else {
    state = QStringLiteral("lull");
  }

  map["current"] = current_phase;
  map["seconds_until_next"] = seconds_until_next < 0.0F
                                  ? -1.0
                                  : static_cast<double>(std::ceil(seconds_until_next));
  map["warning"] = warning;
  map["live_enemies"] = engaged_phase > 0 ? live_units_in_phase(engaged_phase) : 0;
  map["state"] = state;
  map["alerts"] = alerts;
  return map;
}

auto MissionWaveDirector::serialize() const -> QJsonObject {
  QJsonObject root;
  root["elapsed"] = static_cast<double>(m_elapsed);
  root["announced_cleared_phases"] = m_announced_cleared_phases;
  root["announced_all_cleared"] = m_announced_all_cleared;

  QJsonArray waves;
  if (m_waves != nullptr) {
    for (const auto& wave : *m_waves) {
      QJsonObject entry;
      entry["ai_id"] = wave.ai_id;
      entry["phase_index"] = wave.phase_index;
      entry["trigger_time"] = static_cast<double>(wave.trigger_time);
      entry["spawned"] = wave.spawned;
      entry["warned"] = wave.warned;
      entry["cleared"] = wave.cleared;
      entry["cleared_at"] = static_cast<double>(wave.cleared_at);
      QJsonArray ids;
      for (const auto entity_id : wave.spawned_entity_ids) {
        ids.append(static_cast<double>(entity_id));
      }
      entry["spawned_entity_ids"] = ids;
      waves.append(entry);
    }
  }
  root["waves"] = waves;
  return root;
}

void MissionWaveDirector::restore(const QJsonObject& state) {
  m_elapsed = static_cast<float>(state.value("elapsed").toDouble(0.0));
  m_announced_cleared_phases = state.value("announced_cleared_phases").toInt(0);
  m_announced_all_cleared = state.value("announced_all_cleared").toBool(false);

  if (m_waves == nullptr) {
    return;
  }

  const QJsonArray waves = state.value("waves").toArray();
  const auto count = std::min(static_cast<std::size_t>(waves.size()), m_waves->size());
  for (std::size_t i = 0; i < count; ++i) {
    const QJsonObject entry = waves[static_cast<qsizetype>(i)].toObject();
    auto& wave = (*m_waves)[i];
    if (entry.value("ai_id").toString() != wave.ai_id) {
      qWarning() << "Mission wave restore: saved wave layout no longer matches the "
                    "mission definition; waves reset to their authored schedule";
      return;
    }
    wave.spawned = entry.value("spawned").toBool(false);
    wave.warned = entry.value("warned").toBool(false);
    wave.cleared = entry.value("cleared").toBool(false);
    wave.cleared_at = static_cast<float>(entry.value("cleared_at").toDouble(-1.0));
    wave.spawned_entity_ids.clear();
    for (const auto id_value : entry.value("spawned_entity_ids").toArray()) {
      wave.spawned_entity_ids.push_back(
          static_cast<Engine::Core::EntityID>(id_value.toDouble(0.0)));
    }
  }

  m_settled_phases.clear();
  for (const int phase : phase_indices()) {
    if (is_phase_cleared(phase)) {
      m_settled_phases.insert(phase);
    }
  }
  refresh_ready_times();
}

auto MissionWaveDirector::total_wave_count() const -> int {
  return static_cast<int>(phase_indices().size());
}

auto MissionWaveDirector::cleared_wave_count() const -> int {
  int cleared = 0;
  for (const int phase : phase_indices()) {
    if (is_phase_cleared(phase)) {
      cleared++;
    }
  }
  return cleared;
}

} // namespace Game::Mission
