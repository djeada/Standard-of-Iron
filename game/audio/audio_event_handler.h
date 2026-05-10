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
  static constexpr float UNIT_SELECTION_VOLUME = 1.0F;
  static constexpr int UNIT_SELECTION_PRIORITY = 5;
  static constexpr float COMBAT_HIT_VOLUME = 0.6F;
  static constexpr int COMBAT_HIT_PRIORITY = 3;

  AudioEventHandler(Engine::Core::World *world);
  ~AudioEventHandler();

  auto initialize() -> bool;
  void shutdown();

  void load_unit_voice_mapping(const std::string &unit_type,
                               const std::string &sound_id);
  void load_ambient_music(Engine::Core::AmbientState state,
                          const std::string &music_id);

  void set_voice_sound_category(bool use_voice_category);

private:
  void on_unit_selected(const Engine::Core::UnitSelectedEvent &event);
  void
  on_ambient_state_changed(const Engine::Core::AmbientStateChangedEvent &event);
  static void on_audio_trigger(const Engine::Core::AudioTriggerEvent &event);
  static void on_music_trigger(const Engine::Core::MusicTriggerEvent &event);
  static void on_combat_hit(const Engine::Core::CombatHitEvent &event);

  Engine::Core::World *m_world;
  std::unordered_map<std::string, std::string> m_unit_voice_map;
  std::unordered_map<Engine::Core::AmbientState, std::string>
      m_ambient_music_map;

  bool m_use_voice_category{true};

  std::chrono::steady_clock::time_point m_last_selection_sound_time;
  std::string m_last_selection_unit_type;
  static constexpr int SELECTION_SOUND_COOLDOWN_MS = 300;

  Engine::Core::ScopedEventSubscription<Engine::Core::UnitSelectedEvent>
      m_unit_selected_sub;
  Engine::Core::ScopedEventSubscription<Engine::Core::AmbientStateChangedEvent>
      m_ambient_changed_sub;
  Engine::Core::ScopedEventSubscription<Engine::Core::AudioTriggerEvent>
      m_audio_trigger_sub;
  Engine::Core::ScopedEventSubscription<Engine::Core::MusicTriggerEvent>
      m_music_trigger_sub;
  Engine::Core::ScopedEventSubscription<Engine::Core::CombatHitEvent>
      m_combat_hit_sub;

  bool m_initialized{false};
};

} // namespace Game::Audio
