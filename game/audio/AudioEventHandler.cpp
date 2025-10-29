#include "AudioEventHandler.h"
#include "../core/component.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "AudioSystem.h"
#include "core/event_manager.h"
#include "units/spawn_type.h"
#include <chrono>
#include <string>

namespace Game::Audio {

AudioEventHandler::AudioEventHandler(Engine::Core::World *world)
    : m_world(world) {}

AudioEventHandler::~AudioEventHandler() { shutdown(); }

auto AudioEventHandler::initialize() -> bool {
  if (m_initialized) {
    return true;
  }

  m_unitSelectedSub =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitSelectedEvent>(
          [this](const Engine::Core::UnitSelectedEvent &event) {
            onUnitSelected(event);
          });

  m_ambientChangedSub = Engine::Core::ScopedEventSubscription<
      Engine::Core::AmbientStateChangedEvent>(
      [this](const Engine::Core::AmbientStateChangedEvent &event) {
        onAmbientStateChanged(event);
      });

  m_audioTriggerSub =
      Engine::Core::ScopedEventSubscription<Engine::Core::AudioTriggerEvent>(
          [this](const Engine::Core::AudioTriggerEvent &event) {
            onAudioTrigger(event);
          });

  m_musicTriggerSub =
      Engine::Core::ScopedEventSubscription<Engine::Core::MusicTriggerEvent>(
          [this](const Engine::Core::MusicTriggerEvent &event) {
            onMusicTrigger(event);
          });

  m_initialized = true;
  return true;
}

void AudioEventHandler::shutdown() {
  if (!m_initialized) {
    return;
  }

  m_unitSelectedSub.unsubscribe();
  m_ambientChangedSub.unsubscribe();
  m_audioTriggerSub.unsubscribe();
  m_musicTriggerSub.unsubscribe();

  m_unitVoiceMap.clear();
  m_ambientMusicMap.clear();

  m_initialized = false;
}

void AudioEventHandler::loadUnitVoiceMapping(const std::string &unit_type,
                                             const std::string &soundId) {
  m_unitVoiceMap[unit_type] = soundId;
}

void AudioEventHandler::loadAmbientMusic(Engine::Core::AmbientState state,
                                         const std::string &musicId) {
  m_ambientMusicMap[state] = musicId;
}

void AudioEventHandler::setVoiceSoundCategory(bool useVoiceCategory) {
  m_useVoiceCategory = useVoiceCategory;
}

void AudioEventHandler::onUnitSelected(
    const Engine::Core::UnitSelectedEvent &event) {
  if (m_world == nullptr) {
    return;
  }

  auto *entity = m_world->getEntity(event.unit_id);
  if (entity == nullptr) {
    return;
  }

  auto *unit_component = entity->getComponent<Engine::Core::UnitComponent>();
  if (unit_component == nullptr) {
    return;
  }

  std::string const unit_type_str =
      Game::Units::spawn_typeToString(unit_component->spawn_type);
  auto it = m_unitVoiceMap.find(unit_type_str);
  if (it != m_unitVoiceMap.end()) {
    auto now = std::chrono::steady_clock::now();
    auto time_since_last_sound =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_lastSelectionSoundTime)
            .count();

    bool const should_play =
        (time_since_last_sound >= SELECTION_SOUND_COOLDOWN_MS) ||
        (unit_type_str != m_lastSelectionUnitType);

    if (should_play) {
      AudioCategory const category =
          m_useVoiceCategory ? AudioCategory::VOICE : AudioCategory::SFX;
      AudioSystem::getInstance().playSound(it->second, UNIT_SELECTION_VOLUME, false, UNIT_SELECTION_PRIORITY,
                                           category);
      m_lastSelectionSoundTime = now;
      m_lastSelectionUnitType = unit_type_str;
    }
  }
}

void AudioEventHandler::onAmbientStateChanged(
    const Engine::Core::AmbientStateChangedEvent &event) {
  auto it = m_ambientMusicMap.find(event.new_state);
  if (it != m_ambientMusicMap.end()) {
    AudioSystem::getInstance().playMusic(it->second);
  }
}

void AudioEventHandler::onAudioTrigger(
    const Engine::Core::AudioTriggerEvent &event) {
  AudioSystem::getInstance().playSound(event.soundId, event.volume, event.loop,
                                       event.priority);
}

void AudioEventHandler::onMusicTrigger(
    const Engine::Core::MusicTriggerEvent &event) {
  AudioSystem::getInstance().playMusic(event.musicId, event.volume,
                                       event.crossfade);
}

} // namespace Game::Audio
