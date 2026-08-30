#pragma once

#include "game/core/event_manager.h"

namespace Engine::Core {
class World;
}

struct EntityCache;

class AmbientStateManager {
public:
  AmbientStateManager();

  void update(float dt,
              Engine::Core::World* world,
              int local_owner_id,
              const EntityCache& entity_cache,
              const QString& victory_state);

  [[nodiscard]] Engine::Core::AmbientState current_state() const {
    return m_current_ambient_state;
  }

private:
  [[nodiscard]] bool is_player_in_combat(Engine::Core::World* world,
                                         int local_owner_id) const;

  [[nodiscard]] Engine::Core::AmbientState
  observe_state(Engine::Core::World* world,
                int local_owner_id,
                const EntityCache& entity_cache,
                const QString& victory_state) const;

  [[nodiscard]] static float settle_seconds(Engine::Core::AmbientState from,
                                            Engine::Core::AmbientState to);

  Engine::Core::AmbientState m_current_ambient_state =
      Engine::Core::AmbientState::PEACEFUL;

  Engine::Core::AmbientState m_candidate_state = Engine::Core::AmbientState::PEACEFUL;
  float m_candidate_seconds = 0.0F;
  float m_ambient_check_timer = 0.0F;
};
