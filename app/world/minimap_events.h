#pragma once

#include <cstdint>
#include <unordered_map>

#include "game/core/event_manager.h"

class MinimapManager;

namespace Engine::Core {
class World;
}

namespace Game::Session {
class SessionContext;
}

namespace App::ViewModels {
class MinimapViewModel;
}

namespace App::World {

class TroopsUnderAttackAlarm {
public:
  static constexpr float k_cell_metres = 24.0F;
  static constexpr double k_quiet_seconds = 20.0;
  static constexpr double k_cooldown_seconds = 30.0;

  [[nodiscard]] auto note_hit(float world_x, float world_z, double now_seconds) -> bool;
  void reset();

private:
  std::unordered_map<std::int64_t, double> m_last_hit_by_cell;
  double m_last_alarm_seconds = -1.0e9;
};

struct MinimapEventSources {
  MinimapManager* manager = nullptr;
  App::ViewModels::MinimapViewModel* view_model = nullptr;
  Engine::Core::World* world = nullptr;
  Game::Session::SessionContext* session = nullptr;
  const int* local_owner_id = nullptr;
};

class MinimapEvents {
public:
  explicit MinimapEvents(const MinimapEventSources& sources);

  void note_combat_hit(const Engine::Core::CombatHitEvent& event);

  void publish_overlays(float dt);

  void reset();

private:
  void note_unit_died(const Engine::Core::UnitDiedEvent& event);
  void note_shrine_stirred(const Engine::Core::UndeadZoneAwakenedEvent& event);
  void note_barrack_captured(const Engine::Core::BarrackCapturedEvent& event);
  [[nodiscard]] auto minimap_ready() const -> bool;
  void publish_landmarks();

  void sound_troops_under_attack(const Engine::Core::CombatHitEvent& event,
                                 float world_x,
                                 float world_z);
  [[nodiscard]] auto beside_the_commander_in_hand(float world_x,
                                                  float world_z) const -> bool;
  [[nodiscard]] auto
  attacked_by_an_enemy(const Engine::Core::CombatHitEvent& event) const -> bool;

  MinimapEventSources m_sources;
  float m_landmark_poll_accumulator = 0.0F;
  double m_elapsed_seconds = 0.0;
  TroopsUnderAttackAlarm m_troops_alarm;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>
      m_unit_died_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::UndeadZoneAwakenedEvent>
      m_undead_zone_awakened_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::BarrackCapturedEvent>
      m_barrack_captured_subscription;
};

} // namespace App::World
