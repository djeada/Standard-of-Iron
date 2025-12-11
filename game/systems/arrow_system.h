#pragma once
#include "../core/system.h"
#include "../core/world.h"
#include "../game_config.h"
#include <QVector3D>
#include <vector>

namespace Game::Systems {

struct ArrowInstance {
  QVector3D start;
  QVector3D end;
  QVector3D color;
  float t{};
  float speed{};
  bool active{};
  float arc_height{};
  float inv_dist{};
};

class ArrowSystem : public Engine::Core::System {
public:
  ArrowSystem();
  void update(Engine::Core::World *world, float delta_time) override;
  void spawnArrow(const QVector3D &start, const QVector3D &end,
                  const QVector3D &color, float speed = 8.0F);
  [[nodiscard]] auto arrows() const -> const std::vector<ArrowInstance> & {
    return m_arrows;
  }

private:
  std::vector<ArrowInstance> m_arrows;
  ArrowConfig m_config;
};

} // namespace Game::Systems
