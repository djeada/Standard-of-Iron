#pragma once

#include "../core/system.h"
#include "healing_beam.h"
#include <QVector3D>
#include <memory>
#include <vector>

namespace Engine::Core {
class World;
}

namespace Game::Systems {

class HealingBeamSystem : public Engine::Core::System {
public:
  HealingBeamSystem() = default;
  ~HealingBeamSystem() override = default;

  void update(Engine::Core::World *world, float delta_time) override;

  void spawn_beam(const QVector3D &healer_pos, const QVector3D &target_pos,
                  const QVector3D &color = QVector3D(0.3F, 1.0F, 0.5F),
                  float duration = 0.8F);

  auto get_beams() const -> const std::vector<std::unique_ptr<HealingBeam>> & {
    return m_beams;
  }

  auto get_beam_count() const -> size_t { return m_beams.size(); }

  void clear() { m_beams.clear(); }

private:
  std::vector<std::unique_ptr<HealingBeam>> m_beams;
};

} 
