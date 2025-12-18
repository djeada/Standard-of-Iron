#pragma once
#include <QVector3D>
#include <vector>

namespace Engine::Core {
class World;
}

namespace Render::GL {
class Renderer;
class ResourceManager;

struct StoneImpactEffect {
  QVector3D position;
  float start_time{0.0F};
  float duration{5.0F};
  float radius{6.0F};
  float intensity{1.5F};
};

class StoneImpactTracker {
public:
  static auto instance() -> StoneImpactTracker &;

  void add_impact(const QVector3D &position, float current_time,
                  float radius = 6.0F, float intensity = 1.5F);
  void update(float current_time);
  [[nodiscard]] auto impacts() const -> const std::vector<StoneImpactEffect> & {
    return m_impacts;
  }
  void clear() { m_impacts.clear(); }

private:
  StoneImpactTracker() = default;
  std::vector<StoneImpactEffect> m_impacts;
};

void render_combat_dust(Renderer *renderer, ResourceManager *resources,
                        Engine::Core::World *world);

} 
