#include "audio_event_handler.h"

#include <algorithm>
#include <chrono>
#include <random>
#include <string>
#include <utility>

#include "../core/component_core.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "audio_cues.h"
#include "audio_system.h"
#include "core/event_manager.h"
#include "units/spawn_type.h"

namespace Game::Audio {

namespace {

thread_local std::mt19937 g_audio_rng(std::random_device{}());

auto get_volume_variation() -> float {
  std::uniform_real_distribution<float> dist(0.85F, 1.0F);
  return dist(g_audio_rng);
}

auto nation_voice_prefix(Game::Systems::NationID nation_id) -> std::string {
  switch (nation_id) {
  case Game::Systems::NationID::RomanRepublic:
    return "roman";
  case Game::Systems::NationID::Carthage:
    return "carthage";
  case Game::Systems::NationID::IronSepulcher:
    return {};
  }
  return {};
}

auto make_voice_key(Game::Systems::NationID nation_id,
                    const std::string& unit_type) -> std::string {
  const std::string faction = nation_voice_prefix(nation_id);
  if (faction.empty()) {
    return unit_type;
  }
  return faction + "." + unit_type;
}

auto make_manifest_voice_id(Game::Systems::NationID nation_id,
                            const std::string& unit_type) -> std::string {
  const std::string faction = nation_voice_prefix(nation_id);
  if (faction.empty()) {
    return {};
  }
  return "voice." + faction + "." + unit_type;
}

auto resolve_voice_id(const Engine::Core::UnitComponent& unit_component,
                      const std::string& unit_type,
                      const std::unordered_map<std::string, std::string>& voice_map)
    -> std::string {
  const std::string faction_key = make_voice_key(unit_component.nation_id, unit_type);
  if (auto faction_it = voice_map.find(faction_key); faction_it != voice_map.end()) {
    return faction_it->second;
  }

  if (auto generic_it = voice_map.find(unit_type); generic_it != voice_map.end()) {
    return generic_it->second;
  }

  const std::string manifest_voice_id =
      make_manifest_voice_id(unit_component.nation_id, unit_type);
  if (!manifest_voice_id.empty() &&
      AudioSystem::get_instance().has_resource(manifest_voice_id)) {
    return manifest_voice_id;
  }

  const std::string legacy_alias = unit_type + "_voice";
  if (AudioSystem::get_instance().has_resource(legacy_alias)) {
    return legacy_alias;
  }

  return {};
}

auto choose_loaded_variant(const std::vector<std::string>& candidates,
                           const std::string& last_id = {}) -> std::string {
  std::vector<std::string> available_ids;
  available_ids.reserve(candidates.size());
  for (const auto& candidate : candidates) {
    if (AudioSystem::get_instance().has_resource(candidate)) {
      available_ids.push_back(candidate);
    }
  }

  if (available_ids.empty()) {
    return {};
  }

  std::uniform_int_distribution<size_t> dist(0, available_ids.size() - 1U);
  const size_t start_index = dist(g_audio_rng);
  for (size_t offset = 0; offset < available_ids.size(); ++offset) {
    const std::string& candidate =
        available_ids[(start_index + offset) % available_ids.size()];
    if (available_ids.size() == 1U || candidate != last_id) {
      return candidate;
    }
  }

  return available_ids.front();
}

auto ambient_music_group_key(Engine::Core::AmbientState state) -> std::string {
  switch (state) {
  case Engine::Core::AmbientState::PEACEFUL:
    return "ambient.music.peaceful";
  case Engine::Core::AmbientState::TENSE:
    return "ambient.music.tense";
  case Engine::Core::AmbientState::COMBAT:
    return "ambient.music.combat";
  case Engine::Core::AmbientState::VICTORY:
    return "ambient.music.victory";
  case Engine::Core::AmbientState::DEFEAT:
    return "ambient.music.defeat";
  }
  return "ambient.music.peaceful";
}

auto ambient_sfx_group_key(Engine::Core::AmbientState state) -> std::string {
  switch (state) {
  case Engine::Core::AmbientState::PEACEFUL:
    return "ambient.sfx.peaceful";
  case Engine::Core::AmbientState::TENSE:
    return "ambient.sfx.tense";
  case Engine::Core::AmbientState::COMBAT:
    return "ambient.sfx.combat";
  case Engine::Core::AmbientState::VICTORY:
    return "ambient.sfx.victory";
  case Engine::Core::AmbientState::DEFEAT:
    return "ambient.sfx.defeat";
  }
  return "ambient.sfx.peaceful";
}

constexpr const char* k_spawn_voice_group = "voice.unit_spawned";

auto is_siege_engine(Game::Units::SpawnType type) -> bool {
  return type == Game::Units::SpawnType::Catapult ||
         type == Game::Units::SpawnType::Ballista;
}

auto hit_cue_for_attacker(Game::Units::SpawnType type) -> const char* {
  switch (type) {
  case Game::Units::SpawnType::Knight:
  case Game::Units::SpawnType::SkeletonSwordsman:
  case Game::Units::SpawnType::GravePriest:
    return Cue::k_combat_hit_sword;
  case Game::Units::SpawnType::Spearman:
  case Game::Units::SpawnType::HorseSpearman:
    return Cue::k_combat_hit_spear;
  case Game::Units::SpawnType::Archer:
  case Game::Units::SpawnType::SkeletonArcher:
  case Game::Units::SpawnType::HorseArcher:
    return Cue::k_combat_hit_arrow;
  case Game::Units::SpawnType::MountedKnight:
    return Cue::k_combat_hit_cavalry;
  case Game::Units::SpawnType::Elephant:
    return Cue::k_combat_hit_elephant;
  case Game::Units::SpawnType::Catapult:
  case Game::Units::SpawnType::Ballista:
    return Cue::k_combat_hit_siege;
  default:
    return Cue::k_combat_hit_generic;
  }
}

auto impact_cue_for(Game::Units::SpawnType attacker_type,
                    bool target_is_structure) -> const char* {
  if (target_is_structure && !is_siege_engine(attacker_type)) {
    return Cue::k_combat_hit_structure;
  }
  return hit_cue_for_attacker(attacker_type);
}

auto entity_point(const Engine::Core::World* world,
                  Engine::Core::EntityID id,
                  WorldPoint& out) -> bool {
  if (world == nullptr || id == 0) {
    return false;
  }
  const auto* transform = const_cast<Engine::Core::World*>(world)
                              ->try_get<Engine::Core::TransformComponent>(id);
  if (transform == nullptr) {
    return false;
  }
  out = {transform->position.x, transform->position.y, transform->position.z};
  return true;
}

} // namespace

AudioEventHandler::AudioEventHandler(Engine::Core::World* world)
    : m_world(world) {
}

AudioEventHandler::~AudioEventHandler() {
  shutdown();
}

auto AudioEventHandler::initialize() -> bool {
  if (m_initialized) {
    return true;
  }

  m_unit_selected_sub =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitSelectedEvent>(
          [this](const Engine::Core::UnitSelectedEvent& event) {
            on_unit_selected(event);
          });

  m_ambient_changed_sub =
      Engine::Core::ScopedEventSubscription<Engine::Core::AmbientStateChangedEvent>(
          [this](const Engine::Core::AmbientStateChangedEvent& event) {
            on_ambient_state_changed(event);
          });

  m_audio_trigger_sub =
      Engine::Core::ScopedEventSubscription<Engine::Core::AudioTriggerEvent>(
          [this](const Engine::Core::AudioTriggerEvent& event) {
            on_audio_trigger(event);
          });

  m_audio_cue_sub = Engine::Core::ScopedEventSubscription<Engine::Core::AudioCueEvent>(
      [this](const Engine::Core::AudioCueEvent& event) { on_audio_cue(event); });

  m_music_trigger_sub =
      Engine::Core::ScopedEventSubscription<Engine::Core::MusicTriggerEvent>(
          [](const Engine::Core::MusicTriggerEvent& event) {
            on_music_trigger(event);
          });

  m_music_stop_sub =
      Engine::Core::ScopedEventSubscription<Engine::Core::MusicStopEvent>(
          [](const Engine::Core::MusicStopEvent& event) { on_music_stop(event); });

  m_combat_hit_sub =
      Engine::Core::ScopedEventSubscription<Engine::Core::CombatHitEvent>(
          [this](const Engine::Core::CombatHitEvent& event) { on_combat_hit(event); });

  m_unit_spawned_sub =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>(
          [this](const Engine::Core::UnitSpawnedEvent& event) {
            on_unit_spawned(event);
          });

  m_unit_died_sub = Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>(
      [this](const Engine::Core::UnitDiedEvent& event) { on_unit_died(event); });

  m_building_attacked_sub =
      Engine::Core::ScopedEventSubscription<Engine::Core::BuildingAttackedEvent>(
          [this](const Engine::Core::BuildingAttackedEvent& event) {
            on_building_attacked(event);
          });

  m_barrack_captured_sub =
      Engine::Core::ScopedEventSubscription<Engine::Core::BarrackCapturedEvent>(
          [this](const Engine::Core::BarrackCapturedEvent& event) {
            on_barrack_captured(event);
          });

  m_initialized = true;
  return true;
}

void AudioEventHandler::shutdown() {
  if (!m_initialized) {
    return;
  }

  m_unit_selected_sub.unsubscribe();
  m_ambient_changed_sub.unsubscribe();
  m_audio_trigger_sub.unsubscribe();
  m_audio_cue_sub.unsubscribe();
  m_music_stop_sub.unsubscribe();
  m_music_trigger_sub.unsubscribe();
  m_combat_hit_sub.unsubscribe();
  m_unit_spawned_sub.unsubscribe();
  m_unit_died_sub.unsubscribe();
  m_building_attacked_sub.unsubscribe();
  m_barrack_captured_sub.unsubscribe();

  m_unit_voice_map.clear();
  m_ambient_music_map.clear();
  m_ambient_state_sfx_map.clear();
  m_last_sound_group_time.clear();
  m_last_sound_group_id.clear();
  m_current_music_id.clear();

  m_initialized = false;
}

void AudioEventHandler::load_unit_voice_mapping(const std::string& unit_type,
                                                const std::string& sound_id) {
  m_unit_voice_map[unit_type] = sound_id;
}

void AudioEventHandler::load_ambient_music(Engine::Core::AmbientState state,
                                           const std::string& music_id) {
  if (music_id.empty()) {
    m_ambient_music_map.erase(state);
    return;
  }
  m_ambient_music_map[state] = {music_id};
}

void AudioEventHandler::load_ambient_music(Engine::Core::AmbientState state,
                                           const std::vector<std::string>& music_ids) {
  std::vector<std::string> filtered_ids;
  filtered_ids.reserve(music_ids.size());
  for (const auto& music_id : music_ids) {
    if (!music_id.empty()) {
      filtered_ids.push_back(music_id);
    }
  }

  if (filtered_ids.empty()) {
    m_ambient_music_map.erase(state);
    return;
  }

  m_ambient_music_map[state] = std::move(filtered_ids);
}

void AudioEventHandler::load_ambient_state_sfx(
    Engine::Core::AmbientState state, const std::vector<std::string>& sound_ids) {
  std::vector<std::string> filtered_ids;
  filtered_ids.reserve(sound_ids.size());
  for (const auto& sound_id : sound_ids) {
    if (!sound_id.empty()) {
      filtered_ids.push_back(sound_id);
    }
  }

  if (filtered_ids.empty()) {
    m_ambient_state_sfx_map.erase(state);
    return;
  }

  m_ambient_state_sfx_map[state] = std::move(filtered_ids);
}

void AudioEventHandler::set_voice_sound_category(bool use_voice_category) {
  m_use_voice_category = use_voice_category;
}

void AudioEventHandler::set_local_owner_id(int owner_id) {
  m_audience.set_local_owner_id(owner_id);
}

void AudioEventHandler::on_unit_selected(const Engine::Core::UnitSelectedEvent& event) {
  if (m_world == nullptr) {
    return;
  }

  auto* entity = m_world->get_entity(event.unit_id);
  if (entity == nullptr) {
    return;
  }

  auto* unit_component = entity->get_component<Engine::Core::UnitComponent>();
  if (unit_component == nullptr) {
    return;
  }

  std::string const unit_type_str =
      Game::Units::spawn_typeToString(unit_component->spawn_type);
  const std::string sound_id =
      resolve_voice_id(*unit_component, unit_type_str, m_unit_voice_map);
  if (!sound_id.empty()) {
    auto now = std::chrono::steady_clock::now();
    auto time_since_last_sound = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     now - m_last_selection_sound_time)
                                     .count();

    bool const repeats_last_line = sound_id == m_last_selection_sound_id;
    bool const should_play =
        time_since_last_sound >=
        (repeats_last_line ? SELECTION_SOUND_COOLDOWN_MS : SELECTION_VOICE_FLOOR_MS);

    if (should_play) {
      AudioCategory const category =
          m_use_voice_category ? AudioCategory::VOICE : AudioCategory::SFX;
      AudioSystem::get_instance().play_sound(
          sound_id, UNIT_SELECTION_VOLUME, false, UNIT_SELECTION_PRIORITY, category);
      m_last_selection_sound_time = now;
      m_last_selection_sound_id = sound_id;
    }
  }
}

void AudioEventHandler::on_unit_spawned(const Engine::Core::UnitSpawnedEvent& event) {
  if (m_world == nullptr || event.is_initial_spawn ||
      !Game::Units::is_troop_spawn(event.spawn_type)) {
    return;
  }

  if (!m_audience.includes(event.owner_id)) {
    return;
  }

  auto* entity = m_world->get_entity(event.unit_id);
  auto* unit_component = entity != nullptr
                             ? entity->get_component<Engine::Core::UnitComponent>()
                             : nullptr;
  if (unit_component == nullptr) {
    return;
  }

  const std::string unit_type_str = Game::Units::spawn_typeToString(event.spawn_type);
  const std::string sound_id =
      resolve_voice_id(*unit_component, unit_type_str, m_unit_voice_map);
  if (sound_id.empty()) {
    return;
  }

  if (!should_play_sound_group(k_spawn_voice_group, SPAWN_VOICE_COOLDOWN_MS)) {
    return;
  }

  AudioCategory const category =
      m_use_voice_category ? AudioCategory::VOICE : AudioCategory::SFX;
  AudioSystem::get_instance().play_sound(sound_id, 0.85F, false, 4, category);
  mark_sound_group_played(k_spawn_voice_group);
}

void AudioEventHandler::on_unit_died(const Engine::Core::UnitDiedEvent& event) {
  if (!m_audience.involves(event.owner_id, event.killer_owner_id)) {
    return;
  }

  if (Game::Units::is_building_spawn(event.spawn_type)) {
    play_cue(Cue::k_build_building_destroyed);
    return;
  }

  play_cue(Cue::k_combat_death);

  if (m_audience.is_local(event.owner_id)) {
    play_cue(Cue::k_alert_unit_lost);
  }
}

void AudioEventHandler::on_ambient_state_changed(
    const Engine::Core::AmbientStateChangedEvent& event) {
  auto it = m_ambient_music_map.find(event.new_state);
  if (it != m_ambient_music_map.end() && !it->second.empty()) {
    const std::string group_key = ambient_music_group_key(event.new_state);
    const std::string last_id = m_last_sound_group_id[group_key];
    const std::string music_id = choose_loaded_variant(it->second, last_id);

    if (!music_id.empty() && music_id != m_current_music_id) {
      AudioSystem::get_instance().play_music(music_id);
      m_last_sound_group_id[group_key] = music_id;
      m_current_music_id = music_id;
    }
  }

  auto sfx_it = m_ambient_state_sfx_map.find(event.new_state);
  if (sfx_it != m_ambient_state_sfx_map.end() && !sfx_it->second.empty()) {
    play_sound_group(ambient_sfx_group_key(event.new_state),
                     sfx_it->second,
                     0.9F,
                     7,
                     AudioCategory::SFX,
                     2000);
  }

  if (event.new_state == Engine::Core::AmbientState::VICTORY) {
    play_cue(Cue::k_state_victory);
  } else if (event.new_state == Engine::Core::AmbientState::DEFEAT) {
    play_cue(Cue::k_state_defeat);
  }
}

auto AudioEventHandler::should_play_sound_group(const std::string& group_id,
                                                int cooldown_ms) -> bool {
  auto now = std::chrono::steady_clock::now();
  auto it = m_last_sound_group_time.find(group_id);
  if (it != m_last_sound_group_time.end()) {
    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second);
    if (elapsed.count() < cooldown_ms) {
      return false;
    }
  }
  return true;
}

void AudioEventHandler::mark_sound_group_played(const std::string& group_id) {
  m_last_sound_group_time[group_id] = std::chrono::steady_clock::now();
}

void AudioEventHandler::play_sound_group(const std::string& group_id,
                                         const std::vector<std::string>& sound_ids,
                                         float volume,
                                         int priority,
                                         AudioCategory category,
                                         int cooldown_ms) {
  if (sound_ids.empty() || !should_play_sound_group(group_id, cooldown_ms)) {
    return;
  }

  const std::string sound_id =
      choose_loaded_variant(sound_ids, m_last_sound_group_id[group_id]);
  if (sound_id.empty()) {
    return;
  }

  AudioSystem::get_instance().play_sound(sound_id, volume, false, priority, category);
  mark_sound_group_played(group_id);
  m_last_sound_group_id[group_id] = sound_id;
}

void AudioEventHandler::on_building_attacked(
    const Engine::Core::BuildingAttackedEvent& event) {
  if (!m_audience.is_local(event.owner_id) && !m_audience.is_spectating()) {
    return;
  }
  play_cue(Cue::k_alert_base_under_attack);
}

void AudioEventHandler::note_distant_impact(const Game::Audio::WorldPoint& where) {
  const auto listener = AudioSystem::get_instance().listener();
  if (!listener.valid) {
    return;
  }
  if (Game::Audio::spatialize(listener, where).volume_scale > 0.0F) {
    m_distant_impacts.clear();
    return;
  }

  const auto now = std::chrono::steady_clock::now();
  const auto cutoff = now - k_distant_battle_window;
  m_distant_impacts.erase(
      std::remove_if(m_distant_impacts.begin(),
                     m_distant_impacts.end(),
                     [cutoff](const auto& stamp) { return stamp < cutoff; }),
      m_distant_impacts.end());
  m_distant_impacts.push_back(now);

  if (m_distant_impacts.size() <
      static_cast<std::size_t>(k_distant_impacts_for_a_battle)) {
    return;
  }

  m_distant_impacts.clear();
  play_cue(Cue::k_combat_distant_battle);
}

void AudioEventHandler::on_barrack_captured(
    const Engine::Core::BarrackCapturedEvent& event) {
  if (m_audience.is_spectating() || event.new_owner_id == m_audience.local_owner_id()) {
    play_cue(Cue::k_alert_reinforcements);
    return;
  }

  if (event.previous_owner_id == m_audience.local_owner_id()) {
    play_cue(Cue::k_alert_enemy_reinforcements);
  }
}

void AudioEventHandler::on_audio_trigger(const Engine::Core::AudioTriggerEvent& event) {
  if (!m_audience.includes(event.owner_id)) {
    return;
  }
  AudioSystem::get_instance().play_sound(
      event.sound_id, event.volume, event.loop, event.priority);
}

void AudioEventHandler::on_audio_cue(const Engine::Core::AudioCueEvent& event) {
  std::string source = cue_source(event.source_file, event.source_line);
  if (!m_audience.includes(event.owner_id)) {
    CueTrace::instance().record(event.cue_id, {}, CueOutcome::AudienceFiltered, source);
    return;
  }
  if (event.positioned) {
    const WorldPoint position{event.x, event.y, event.z};
    play_cue_from(event.cue_id, event.volume_scale, std::move(source), &position);
    return;
  }
  play_cue_from(event.cue_id, event.volume_scale, std::move(source));
}

void AudioEventHandler::on_music_stop(const Engine::Core::MusicStopEvent&) {
  AudioSystem::get_instance().stop_music();
}

void AudioEventHandler::on_music_trigger(const Engine::Core::MusicTriggerEvent& event) {
  AudioSystem::get_instance().play_music(event.music_id,
                                         event.volume,
                                         event.crossfade
                                             ? Game::Audio::MusicTransition::Crossfade
                                             : Game::Audio::MusicTransition::Immediate);
}

void AudioEventHandler::on_combat_hit(const Engine::Core::CombatHitEvent& event) {
  if (!m_audience.involves(event.attacker_owner_id, event.target_owner_id)) {
    return;
  }

  float const volume = COMBAT_HIT_VOLUME * get_volume_variation();
  const char* const impact =
      impact_cue_for(event.attacker_type, event.target_is_structure);

  WorldPoint where;
  const bool located = entity_point(m_world, event.target_id, where);
  if (located) {
    play_cue_at(impact, where, volume);
    note_distant_impact(where);
  } else {
    play_cue(impact, volume);
  }

  if (event.is_killing_blow && !event.target_is_structure) {
    if (located) {
      play_cue_at(Cue::k_combat_death, where, get_volume_variation());
    } else {
      play_cue(Cue::k_combat_death, get_volume_variation());
    }
  }
}

} // namespace Game::Audio
