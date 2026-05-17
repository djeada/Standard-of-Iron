#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>

#include "../core/event_manager.h"

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

  AudioEventHandler(Engine::Core::World* world);
  ~AudioEventHandler();

  auto initialize() -> bool;
  void shutdown();

  void load_unit_voice_mapping(const std::string& unit_type,
                               const std::string& sound_id);
  void load_ambient_music(Engine::Core::AmbientState state,
                          const std::string& music_id);

  void set_voice_sound_category(bool use_voice_category);

private:
  void on_unit_selected(const Engine::Core::UnitSelectedEvent& event);
  void on_unit_spawned(const Engine::Core::UnitSpawnedEvent& event);
  void on_unit_died(const Engine::Core::UnitDiedEvent& event);
  void on_ambient_state_changed(const Engine::Core::AmbientStateChangedEvent& event);
  void on_building_attacked(const Engine::Core::BuildingAttackedEvent& event);
  void on_barrack_captured(const Engine::Core::BarrackCapturedEvent& event);
  static void on_audio_trigger(const Engine::Core::AudioTriggerEvent& event);
  static void on_music_trigger(const Engine::Core::MusicTriggerEvent& event);
  static void on_combat_hit(const Engine::Core::CombatHitEvent& event);

  auto should_play_alert(const std::string& sound_id, int cooldown_ms) -> bool;
  void play_alert_sound(const std::string& sound_id,
                        float volume,
                        int priority,
                        int cooldown_ms);

  Engine::Core::World* m_world;
  std::unordered_map<std::string, std::string> m_unit_voice_map;
  std::unordered_map<Engine::Core::AmbientState, std::string> m_ambient_music_map;
  std::unordered_map<std::string, std::chrono::steady_clock::time_point>
      m_last_alert_sound_time;

  bool m_use_voice_category{true};

  std::chrono::steady_clock::time_point m_last_selection_sound_time;
  std::string m_last_selection_sound_id;
  static constexpr int SELECTION_SOUND_COOLDOWN_MS = 300;

  Engine::Core::ScopedEventSubscription<Engine::Core::UnitSelectedEvent>
      m_unit_selected_sub;
  Engine::Core::ScopedEventSubscription<Engine::Core::AmbientStateChangedEvent>
      m_ambient_changed_sub;
  Engine::Core::ScopedEventSubscription<Engine::Core::AudioTriggerEvent>
      m_audio_trigger_sub;
  Engine::Core::ScopedEventSubscription<Engine::Core::MusicTriggerEvent>
      m_music_trigger_sub;
  Engine::Core::ScopedEventSubscription<Engine::Core::CombatHitEvent> m_combat_hit_sub;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>
      m_unit_spawned_sub;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent> m_unit_died_sub;
  Engine::Core::ScopedEventSubscription<Engine::Core::BuildingAttackedEvent>
      m_building_attacked_sub;
  Engine::Core::ScopedEventSubscription<Engine::Core::BarrackCapturedEvent>
      m_barrack_captured_sub;

  bool m_initialized{false};
};

} // namespace Game::Audio
