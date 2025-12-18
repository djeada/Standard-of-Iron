#include "healing_beam_system.h"
#include "../core/world.h"
#include <QDebug>
#include <algorithm>

namespace Game::Systems {

void HealingBeamSystem::update(Engine::Core::World *, float delta_time) {

  for (auto &beam : m_beams) {
    if (beam && beam->is_active()) {
      beam->update(delta_time);
    }
  }

  m_beams.erase(std::remove_if(m_beams.begin(), m_beams.end(),
                               [](const std::unique_ptr<HealingBeam> &beam) {
                                 return !beam || !beam->is_active();
                               }),
                m_beams.end());
}

void HealingBeamSystem::spawn_beam(const QVector3D &healer_pos,
                                   const QVector3D &target_pos,
                                   const QVector3D &color, float duration) {
  m_beams.push_back(
      std::make_unique<HealingBeam>(healer_pos, target_pos, color, duration));
}

} // namespace Game::Systems
