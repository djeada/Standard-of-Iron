#pragma once
#include <QVector3D>

#include <unordered_map>
#include <vector>

#include "../../game/core/component.h"
#include "../../game/core/entity.h"

namespace Engine::Core {
class World;
}

namespace Render::GL {
class Renderer;
class ResourceManager;

struct StoneImpactEffect {
  QVector3D position;
  QVector3D color{0.75F, 0.65F, 0.45F};
  float start_time{0.0F};
  float duration{5.0F};
  float radius{6.0F};
  float intensity{1.5F};
};

class StoneImpactTracker {
public:
  static auto instance() -> StoneImpactTracker&;

  void add_impact(const QVector3D& position,
                  float current_time,
                  float radius = 6.0F,
                  float intensity = 1.5F,
                  const QVector3D& color = QVector3D(0.75F, 0.65F, 0.45F));
  void update(float current_time);
  [[nodiscard]] auto impacts() const -> const std::vector<StoneImpactEffect>& {
    return m_impacts;
  }
  void clear() { m_impacts.clear(); }

private:
  StoneImpactTracker() = default;
  std::vector<StoneImpactEffect> m_impacts;
};

void render_combat_dust(Renderer* renderer,
                        ResourceManager* resources,
                        Engine::Core::World* world);

struct TelegraphEntry {
  float last_pos_x{0.0F};
  float last_pos_z{0.0F};
  float base_y{0.0F};
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
  void render(Renderer* renderer,
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
