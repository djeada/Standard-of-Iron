#pragma once
#include <QVector3D>

#include <cstdint>
#include <vector>

#include "../core/system.h"
#include "../core/world.h"
#include "../game_config.h"
#include "arrow_instance.h"
#include "arrow_visual_profile.h"

namespace Game::Systems {

class ArrowSystem : public Engine::Core::System {
public:
  ArrowSystem();
  void update(Engine::Core::World* world, float delta_time) override;

  [[nodiscard]] auto access() const -> Engine::Core::SystemAccess override;
  void spawn_arrow(const QVector3D& start,
                   const QVector3D& end,
                   const QVector3D& color,
                   float speed = 8.0F,
                   ArrowVisualStyle style = ArrowVisualStyle::Focused);
  [[nodiscard]] auto arrows() const -> const std::vector<ArrowInstance>& {
    return m_arrows;
  }

private:
  std::vector<ArrowInstance> m_arrows;
  ArrowConfig m_config;
  std::uint32_t m_spawn_sequence = 0;
};

} // namespace Game::Systems
