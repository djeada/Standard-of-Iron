#include "AudioEventHandler.h"
#include "../core/component.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "AudioSystem.h"

namespace Game {
namespace Audio {

AudioEventHandler::AudioEventHandler(Engine::Core::World *world)
    : m_world(world), m_initialized(false) {}

AudioEventHandler::~AudioEventHandler() { shutdown(); }

bool AudioEventHandler::initialize() {
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

void AudioEventHandler::loadUnitVoiceMapping(const std::string &unitType,
                                             const std::string &soundId) {
  m_unitVoiceMap[unitType] = soundId;
}

void AudioEventHandler::loadAmbientMusic(Engine::Core::AmbientState state,
                                         const std::string &musicId) {
  m_ambientMusicMap[state] = musicId;
}

void AudioEventHandler::onUnitSelected(
    const Engine::Core::UnitSelectedEvent &event) {
  if (!m_world) {
    return;
  }

  auto *entity = m_world->getEntity(event.unitId);
  if (!entity) {
    return;
  }

  auto *unitComponent = entity->getComponent<Engine::Core::UnitComponent>();
  if (!unitComponent) {
    return;
  }

  auto it = m_unitVoiceMap.find(unitComponent->unitType);
  if (it != m_unitVoiceMap.end()) {
    AudioSystem::getInstance().playSound(it->second);
  }
}

void AudioEventHandler::onAmbientStateChanged(
    const Engine::Core::AmbientStateChangedEvent &event) {
  auto it = m_ambientMusicMap.find(event.newState);
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

} // namespace Audio
} // namespace Game
