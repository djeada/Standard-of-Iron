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

  auto const before = m_beams.size();
  m_beams.erase(std::remove_if(m_beams.begin(), m_beams.end(),
                               [](const std::unique_ptr<HealingBeam> &beam) {
                                 return !beam || !beam->is_active();
                               }),
                m_beams.end());
  auto const removed = before - m_beams.size();
  if (removed > 0) {
    qDebug() << "HealingBeamSystem removed inactive beams:" << removed
             << "active:" << m_beams.size();
  }
}

void HealingBeamSystem::spawn_beam(const QVector3D &healer_pos,
                                   const QVector3D &target_pos,
                                   const QVector3D &color, float duration) {
  m_beams.push_back(
      std::make_unique<HealingBeam>(healer_pos, target_pos, color, duration));
  qDebug() << "HealingBeamSystem spawned beam. healer" << healer_pos << "target"
           << target_pos << "color" << color << "duration" << duration
           << "total active" << m_beams.size();
}

} // namespace Game::Systems
