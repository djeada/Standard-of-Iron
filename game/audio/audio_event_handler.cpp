#include "audio_event_handler.h"

#include <chrono>
#include <random>
#include <string>

#include "../core/component.h"
#include "../core/entity.h"
#include "../core/world.h"
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

auto get_hit_sound_for_type(Game::Units::SpawnType type) -> std::string {
  switch (type) {
  case Game::Units::SpawnType::Knight:
  case Game::Units::SpawnType::SkeletonSwordsman:
  case Game::Units::SpawnType::GravePriest:
    return "combat_hit_sword";
  case Game::Units::SpawnType::Spearman:
  case Game::Units::SpawnType::HorseSpearman:
    return "combat_hit_spear";
  case Game::Units::SpawnType::Archer:
  case Game::Units::SpawnType::SkeletonArcher:
  case Game::Units::SpawnType::HorseArcher:
    return "combat_hit_arrow";
  case Game::Units::SpawnType::MountedKnight:
    return "combat_hit_cavalry";
  case Game::Units::SpawnType::Elephant:
    return "combat_hit_elephant";
  case Game::Units::SpawnType::Catapult:
  case Game::Units::SpawnType::Ballista:
    return "combat_hit_siege";
  default:
    return "combat_hit_generic";
  }
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

  m_music_trigger_sub =
      Engine::Core::ScopedEventSubscription<Engine::Core::MusicTriggerEvent>(
          [this](const Engine::Core::MusicTriggerEvent& event) {
            on_music_trigger(event);
          });

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
  m_music_trigger_sub.unsubscribe();
  m_combat_hit_sub.unsubscribe();
  m_unit_spawned_sub.unsubscribe();
  m_unit_died_sub.unsubscribe();
  m_building_attacked_sub.unsubscribe();
  m_barrack_captured_sub.unsubscribe();

  m_unit_voice_map.clear();
  m_ambient_music_map.clear();
  m_last_alert_sound_time.clear();

  m_initialized = false;
}

void AudioEventHandler::load_unit_voice_mapping(const std::string& unit_type,
                                                const std::string& sound_id) {
  m_unit_voice_map[unit_type] = sound_id;
}

void AudioEventHandler::load_ambient_music(Engine::Core::AmbientState state,
                                           const std::string& music_id) {
  m_ambient_music_map[state] = music_id;
}

void AudioEventHandler::set_voice_sound_category(bool use_voice_category) {
  m_use_voice_category = use_voice_category;
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

    bool const should_play = (time_since_last_sound >= SELECTION_SOUND_COOLDOWN_MS) ||
                             (sound_id != m_last_selection_sound_id);

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

  AudioCategory const category =
      m_use_voice_category ? AudioCategory::VOICE : AudioCategory::SFX;
  AudioSystem::get_instance().play_sound(sound_id, 0.85F, false, 4, category);
}

void AudioEventHandler::on_unit_died(const Engine::Core::UnitDiedEvent& event) {
  if (!Game::Units::is_troop_spawn(event.spawn_type)) {
    return;
  }

  AudioSystem::get_instance().play_sound(
      "combat_death", 0.75F, false, 6, AudioCategory::SFX);
}

void AudioEventHandler::on_ambient_state_changed(
    const Engine::Core::AmbientStateChangedEvent& event) {
  auto it = m_ambient_music_map.find(event.new_state);
  if (it != m_ambient_music_map.end()) {
    AudioSystem::get_instance().play_music(it->second);
  }
}

auto AudioEventHandler::should_play_alert(const std::string& sound_id,
                                          int cooldown_ms) -> bool {
  auto now = std::chrono::steady_clock::now();
  auto it = m_last_alert_sound_time.find(sound_id);
  if (it != m_last_alert_sound_time.end()) {
    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second);
    if (elapsed.count() < cooldown_ms) {
      return false;
    }
  }
  m_last_alert_sound_time[sound_id] = now;
  return true;
}

void AudioEventHandler::play_alert_sound(const std::string& sound_id,
                                         float volume,
                                         int priority,
                                         int cooldown_ms) {
  if (!should_play_alert(sound_id, cooldown_ms)) {
    return;
  }

  AudioSystem::get_instance().play_sound(
      sound_id, volume, false, priority, AudioCategory::SFX);
}

void AudioEventHandler::on_building_attacked(
    const Engine::Core::BuildingAttackedEvent& event) {
  (void)event;
  play_alert_sound("enemy_spotted_horn", 0.85F, 7, 1500);
}

void AudioEventHandler::on_barrack_captured(
    const Engine::Core::BarrackCapturedEvent& event) {
  (void)event;
  play_alert_sound("reinforcements_arrived", 0.90F, 8, 3000);
}

void AudioEventHandler::on_audio_trigger(const Engine::Core::AudioTriggerEvent& event) {
  AudioSystem::get_instance().play_sound(
      event.sound_id, event.volume, event.loop, event.priority);
}

void AudioEventHandler::on_music_trigger(const Engine::Core::MusicTriggerEvent& event) {
  AudioSystem::get_instance().play_music(event.music_id, event.volume, event.crossfade);
}

void AudioEventHandler::on_combat_hit(const Engine::Core::CombatHitEvent& event) {
  std::string const sound_id = get_hit_sound_for_type(event.attacker_type);
  float const volume = COMBAT_HIT_VOLUME * get_volume_variation();

  AudioSystem::get_instance().play_sound(
      sound_id, volume, false, COMBAT_HIT_PRIORITY, AudioCategory::SFX);

  if (event.is_killing_blow) {
    AudioSystem::get_instance().play_sound("combat_death",
                                           volume * 0.9F,
                                           false,
                                           COMBAT_HIT_PRIORITY + 1,
                                           AudioCategory::SFX);
  }
}

} // namespace Game::Audio
