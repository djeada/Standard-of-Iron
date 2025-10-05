#pragma once
#include "../core/system.h"
#include "../core/world.h"
#include <QVector3D>
#include <vector>

namespace Game::Systems {

struct ArrowInstance {
  QVector3D start;
  QVector3D end;
  QVector3D color;
  float t;
  float speed;
  bool active;
  float arcHeight;
};

class ArrowSystem : public Engine::Core::System {
public:
  ArrowSystem();
  void update(Engine::Core::World *world, float deltaTime) override;
  void spawnArrow(const QVector3D &start, const QVector3D &end,
                  const QVector3D &color, float speed = 8.0f);
  const std::vector<ArrowInstance> &arrows() const { return m_arrows; }

private:
  std::vector<ArrowInstance> m_arrows;
};

} // namespace Game::Systems
