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

auto get_hit_sound_for_type(Game::Units::SpawnType type) -> std::string {
  switch (type) {
  case Game::Units::SpawnType::Knight:
  case Game::Units::SpawnType::MountedKnight:
    return "combat_hit_sword";
  case Game::Units::SpawnType::Spearman:
  case Game::Units::SpawnType::HorseSpearman:
    return "combat_hit_spear";
  case Game::Units::SpawnType::Archer:
  case Game::Units::SpawnType::HorseArcher:
    return "combat_hit_arrow";
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
          [](const Engine::Core::CombatHitEvent& event) { on_combat_hit(event); });

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

  m_unit_voice_map.clear();
  m_ambient_music_map.clear();

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
  auto it = m_unit_voice_map.find(unit_type_str);
  if (it != m_unit_voice_map.end()) {
    auto now = std::chrono::steady_clock::now();
    auto time_since_last_sound = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     now - m_last_selection_sound_time)
                                     .count();

    bool const should_play = (time_since_last_sound >= SELECTION_SOUND_COOLDOWN_MS) ||
                             (unit_type_str != m_last_selection_unit_type);

    if (should_play) {
      AudioCategory const category =
          m_use_voice_category ? AudioCategory::VOICE : AudioCategory::SFX;
      AudioSystem::get_instance().play_sound(
          it->second, UNIT_SELECTION_VOLUME, false, UNIT_SELECTION_PRIORITY, category);
      m_last_selection_sound_time = now;
      m_last_selection_unit_type = unit_type_str;
    }
  }
}

void AudioEventHandler::on_ambient_state_changed(
    const Engine::Core::AmbientStateChangedEvent& event) {
  auto it = m_ambient_music_map.find(event.new_state);
  if (it != m_ambient_music_map.end()) {
    AudioSystem::get_instance().play_music(it->second);
  }
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
