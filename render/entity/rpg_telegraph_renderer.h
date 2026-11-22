#pragma once

#include <QVector3D>

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "game/core/component_combat.h"
#include "game/core/entity.h"

namespace Engine::Core {
class World;
}

namespace Render::GL {

class FrameSubmitter;

struct TelegraphEntry {
  float last_pos_x{0.0F};
  float last_pos_z{0.0F};
  float base_y{0.0F};
  std::uint16_t soldier_slot{
      Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot};
  Engine::Core::CombatAnimationState prev_state{
      Engine::Core::CombatAnimationState::Idle};
};

struct StrikeFlash {
  QVector3D pos;
  float start_time{0.0F};
  static constexpr float k_duration = 0.35F;
};

class RpgTelegraphRenderer {
public:
  void render(FrameSubmitter* renderer,
              Engine::Core::World* world,
              Engine::Core::EntityID commander_id,
              Engine::Core::EntityID locked_target_id,
              float anim_time);
  void clear();

private:
  std::unordered_map<Engine::Core::EntityID, TelegraphEntry> m_cache;
  std::vector<StrikeFlash> m_strike_flashes;
};

} // namespace Render::GL
