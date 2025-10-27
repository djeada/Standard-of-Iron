#pragma once

#include "../core/event_manager.h"
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>

class AudioSystem;

namespace Engine::Core {
class World;
}

namespace Game::Audio {

class AudioEventHandler {
public:
  AudioEventHandler(Engine::Core::World *world);
  ~AudioEventHandler();

  auto initialize() -> bool;
  void shutdown();

  void loadUnitVoiceMapping(const std::string &unit_type,
                            const std::string &soundId);
  void loadAmbientMusic(Engine::Core::AmbientState state,
                        const std::string &musicId);

  void setVoiceSoundCategory(bool useVoiceCategory);

private:
  void onUnitSelected(const Engine::Core::UnitSelectedEvent &event);
  void
  onAmbientStateChanged(const Engine::Core::AmbientStateChangedEvent &event);
  static void onAudioTrigger(const Engine::Core::AudioTriggerEvent &event);
  static void onMusicTrigger(const Engine::Core::MusicTriggerEvent &event);

  Engine::Core::World *m_world;
  std::unordered_map<std::string, std::string> m_unitVoiceMap;
  std::unordered_map<Engine::Core::AmbientState, std::string> m_ambientMusicMap;

  bool m_useVoiceCategory{true};

  std::chrono::steady_clock::time_point m_lastSelectionSoundTime;
  std::string m_lastSelectionUnitType;
  static constexpr int SELECTION_SOUND_COOLDOWN_MS = 300;

  Engine::Core::ScopedEventSubscription<Engine::Core::UnitSelectedEvent>
      m_unitSelectedSub;
  Engine::Core::ScopedEventSubscription<Engine::Core::AmbientStateChangedEvent>
      m_ambientChangedSub;
  Engine::Core::ScopedEventSubscription<Engine::Core::AudioTriggerEvent>
      m_audioTriggerSub;
  Engine::Core::ScopedEventSubscription<Engine::Core::MusicTriggerEvent>
      m_musicTriggerSub;

  bool m_initialized{false};
};

} // namespace Game::Audio
