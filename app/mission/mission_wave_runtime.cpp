#include "app/mission/mission_wave_runtime.h"

#include "game/core/world.h"
#include "game/mission/campaign_manager.h"
#include "game/systems/match_snapshot.h"
#include "game/systems/victory_service.h"
#include "game/util/asset_text.h"

namespace App::Mission {

void MissionWaveRuntime::reset() {
  m_elapsed = 0.0F;
  m_waves.clear();
  m_events.clear();
  m_director.reset();
}

void MissionWaveRuntime::bind_after_setup(
    const MissionWaveBinding& binding,
    std::vector<Game::Mission::PendingMissionWave> waves,
    std::vector<Game::Mission::PendingMissionEvent> events) {
  m_waves = std::move(waves);
  m_events = std::move(events);
  m_director.bind(&m_waves, binding.world);
  m_director.set_elapsed(m_elapsed);
  if (binding.victory_service != nullptr) {
    binding.victory_service->set_mission_wave_query(&m_director);
  }
}

void MissionWaveRuntime::restore(const MissionWaveBinding& binding,
                                 const QJsonObject& wave_state) {
  m_waves.clear();
  m_events.clear();
  m_director.reset();

  if (binding.world == nullptr || binding.campaign == nullptr ||
      binding.level == nullptr ||
      !binding.campaign->current_mission_definition().has_value()) {
    return;
  }

  const auto& mission = *binding.campaign->current_mission_definition();

  const QVector3D defense_reference =
      Game::Mission::resolve_defense_reference(*binding.world, binding.local_owner_id);

  m_waves = Game::Mission::build_pending_mission_waves(
      {.mission = mission,
       .mission_difficulty = binding.campaign->current_mission_context().difficulty,
       .level = *binding.level,
       .defense_reference_world_position = defense_reference});

  m_events = Game::Mission::build_pending_mission_events(mission);

  m_director.bind(&m_waves, binding.world);
  m_director.restore(wave_state);
  m_elapsed = m_director.elapsed();
  for (auto& event : m_events) {
    event.fired = m_elapsed >= event.trigger_time;
  }

  if (binding.victory_service != nullptr) {
    binding.victory_service->set_mission_wave_query(&m_director);
  }
}

auto MissionWaveRuntime::fire_due_events() -> QStringList {
  QStringList announcements;
  for (auto& event : m_events) {
    if (event.fired || m_elapsed < event.trigger_time) {
      continue;
    }
    event.fired = true;
    announcements.append(
        Game::Util::tr_asset(Game::Util::k_missions_context, event.text));
  }
  return announcements;
}

auto MissionWaveRuntime::advance(const MissionWaveBinding& binding,
                                 float delta_seconds,
                                 bool hold_clock) -> MissionFrameEffects {
  MissionFrameEffects effects;
  if (delta_seconds <= 0.0F || binding.world == nullptr || binding.level == nullptr ||
      hold_clock) {
    return effects;
  }

  m_elapsed += delta_seconds;
  effects.announcements = fire_due_events();

  if (m_waves.empty()) {
    return effects;
  }

  m_director.set_elapsed(m_elapsed);
  const auto director_effects = m_director.advance();

  bool spawned_any = false;
  for (const auto index : director_effects.waves_to_spawn) {
    const auto spawn_effects =
        m_spawner.spawn({*binding.world, *binding.level, m_elapsed}, m_waves[index]);
    for (const auto& announcement : spawn_effects.mission_announcements) {
      effects.announcements.append(announcement);
    }
    m_director.note_spawned(index, spawn_effects.spawned_entity_ids);
    spawned_any = true;
  }

  for (const auto& announcement : director_effects.announcements) {
    effects.announcements.append(announcement);
  }
  for (const auto& cue : director_effects.audio_cues) {
    effects.audio_cues.append(cue);
  }

  if (!director_effects.reward.empty()) {
    effects.reward = director_effects.reward;
    effects.reward_granted = true;
  }

  effects.wave_status_changed = director_effects.status_changed || spawned_any;
  effects.owner_info_changed = spawned_any;
  return effects;
}

} // namespace App::Mission
