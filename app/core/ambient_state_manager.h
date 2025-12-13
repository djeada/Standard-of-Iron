#pragma once

#include "game/core/event_manager.h"

namespace Engine::Core {
class World;
}

struct EntityCache {
  int player_troop_count = 0;
  bool player_barracks_alive = false;
  bool enemy_barracks_alive = false;
  int enemy_barracks_count = 0;

  void reset() {
    player_troop_count = 0;
    player_barracks_alive = false;
    enemy_barracks_alive = false;
    enemy_barracks_count = 0;
  }
};

class AmbientStateManager {
public:
  AmbientStateManager();

  void update(float dt, Engine::Core::World *world, int local_owner_id,
              const EntityCache &entity_cache, const QString &victory_state);

  [[nodiscard]] Engine::Core::AmbientState current_state() const {
    return m_current_ambient_state;
  }

private:
  [[nodiscard]] bool is_player_in_combat(Engine::Core::World *world,
                                         int local_owner_id) const;

  Engine::Core::AmbientState m_current_ambient_state =
      Engine::Core::AmbientState::PEACEFUL;
  float m_ambient_check_timer = 0.0F;
};
