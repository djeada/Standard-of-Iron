#pragma once

#include <vector>
#include <QVector3D>

namespace Engine { namespace Core {
class World;
using EntityID = unsigned int;
struct MovementComponent;
} }

namespace Game { namespace Systems {

class CommandService {
public:
    // Ensure movement components exist and assign targets for each entity to corresponding positions (same size vectors expected).
    static void moveUnits(Engine::Core::World& world,
                          const std::vector<Engine::Core::EntityID>& units,
                          const std::vector<QVector3D>& targets);
};

} } // namespace Game::Systems
