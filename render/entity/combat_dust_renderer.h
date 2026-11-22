#pragma once
#include <QVector3D>

#include <unordered_map>
#include <vector>

#include "game/core/component_combat.h"
#include "game/core/entity.h"

namespace Engine::Core {
class World;
}

namespace Render::GL {
class FrameSubmitter;
class DrawResources;

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

void render_combat_dust(FrameSubmitter* renderer,
                        const DrawResources* resources,
                        Engine::Core::World* world);

void render_blood_stains(FrameSubmitter* renderer,
                         const DrawResources* resources,
                         Engine::Core::World* world);

} // namespace Render::GL
