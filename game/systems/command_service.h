#pragma once

#include <QVector3D>
#include <vector>

namespace Engine {
namespace Core {
class World;
using EntityID = unsigned int;
struct MovementComponent;
} // namespace Core
} // namespace Engine

namespace Game {
namespace Systems {

class CommandService {
public:
  static void moveUnits(Engine::Core::World &world,
                        const std::vector<Engine::Core::EntityID> &units,
                        const std::vector<QVector3D> &targets);

  static void attackTarget(Engine::Core::World &world,
                           const std::vector<Engine::Core::EntityID> &units,
                           Engine::Core::EntityID targetId,
                           bool shouldChase = true);
};

} // namespace Systems
} // namespace Game
