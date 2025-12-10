#include "arrow_system.h"
#include "../../render/geom/arrow.h"
#include "../../render/scene_renderer.h"
#include <algorithm>
#include <qvectornd.h>

namespace Game::Systems {

ArrowSystem::ArrowSystem() : m_config(GameConfig::instance().arrow()) {}

void ArrowSystem::spawnArrow(const QVector3D &start, const QVector3D &end,
                             const QVector3D &color, float speed) {
  ArrowInstance a;
  a.start = start;
  a.end = end;
  a.color = color;
  a.t = 0.0F;
  a.speed = speed;
  a.active = true;
  QVector3D const delta = end - start;
  float const dist = delta.length();
  a.arcHeight = std::clamp(m_config.arcHeightMultiplier * dist,
                           m_config.arcHeightMin, m_config.arcHeightMax);

  a.invDist = (dist > 0.001F) ? (1.0F / dist) : 1.0F;
  m_arrows.push_back(a);
}

void ArrowSystem::update(Engine::Core::World *, float delta_time) {
  for (auto &arrow : m_arrows) {
    if (!arrow.active) {
      continue;
    }

    arrow.t += delta_time * arrow.speed * arrow.invDist;
    if (arrow.t >= 1.0F) {
      arrow.t = 1.0F;
      arrow.active = false;
    }
  }

  m_arrows.erase(
      std::remove_if(m_arrows.begin(), m_arrows.end(),
                     [](const ArrowInstance &a) { return !a.active; }),
      m_arrows.end());
}

} // namespace Game::Systems
