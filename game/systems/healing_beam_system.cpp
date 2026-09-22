#include "healing_beam_system.h"

#include <QDebug>

#include <algorithm>

#include "../core/world.h"

namespace Game::Systems {

void HealingBeamSystem::update(Engine::Core::World* world, float delta_time) {

  for (auto& beam : m_beams) {
    if (beam && beam->is_active()) {
      beam->update(delta_time);
    }
  }

  m_beams.erase(std::remove_if(m_beams.begin(),
                               m_beams.end(),
                               [](const std::unique_ptr<HealingBeam>& beam) {
                                 return !beam || !beam->is_active();
                               }),
                m_beams.end());

  if (world != nullptr) {
    auto& published = world->render_effects_frame().healing_beams;
    published.clear();
    published.reserve(m_beams.size());
    for (const auto& beam : m_beams) {
      if (!beam || !beam->is_active()) {
        continue;
      }
      published.push_back({.start = beam->get_start(),
                           .end = beam->get_end(),
                           .color = beam->get_color(),
                           .progress = beam->get_progress(),
                           .beam_width = beam->get_beam_width(),
                           .intensity = beam->get_intensity()});
    }
  }
}

void HealingBeamSystem::spawn_beam(const QVector3D& healer_pos,
                                   const QVector3D& target_pos,
                                   const QVector3D& color,
                                   float duration) {
  m_beams.push_back(
      std::make_unique<HealingBeam>(healer_pos, target_pos, color, duration));
}

auto HealingBeamSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(Reads<>{}, Writes<>{});
}

} // namespace Game::Systems
