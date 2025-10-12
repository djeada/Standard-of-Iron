#include "arrow_system.h"
#include "../../render/geom/arrow.h"
#include "../../render/gl/resources.h"
#include "../../render/scene_renderer.h"
#include <algorithm>

namespace Game::Systems {

ArrowSystem::ArrowSystem() : m_config(GameConfig::instance().arrow()) {}

void ArrowSystem::spawnArrow(const QVector3D &start, const QVector3D &end,
                             const QVector3D &color, float speed) {
  ArrowInstance a;
  a.start = start;
  a.end = end;
  a.color = color;
  a.t = 0.0f;
  a.speed = speed;
  a.active = true;
  QVector3D delta = end - start;
  float dist = delta.length(); // Only one sqrt needed here
  a.arcHeight = std::clamp(m_config.arcHeightMultiplier * dist,
                           m_config.arcHeightMin, m_config.arcHeightMax);
  // Store invDist to avoid recalculating in update loop
  a.invDist = (dist > 0.001f) ? (1.0f / dist) : 1.0f;
  m_arrows.push_back(a);
}

void ArrowSystem::update(Engine::Core::World *world, float deltaTime) {
  for (auto &arrow : m_arrows) {
    if (!arrow.active)
      continue;
    // Use precomputed invDist to avoid sqrt in hot loop
    arrow.t += deltaTime * arrow.speed * arrow.invDist;
    if (arrow.t >= 1.0f) {
      arrow.t = 1.0f;
      arrow.active = false;
    }
  }

  m_arrows.erase(
      std::remove_if(m_arrows.begin(), m_arrows.end(),
                     [](const ArrowInstance &a) { return !a.active; }),
      m_arrows.end());
}

} // namespace Game::Systems
