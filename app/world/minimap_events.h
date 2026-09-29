#pragma once

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

  MinimapEventSources m_sources;
  float m_landmark_poll_accumulator = 0.0F;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>
      m_unit_died_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::UndeadZoneAwakenedEvent>
      m_undead_zone_awakened_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::BarrackCapturedEvent>
      m_barrack_captured_subscription;
};

} // namespace App::World
