#include "command_service.h"
#include "../core/world.h"
#include "../core/component.h"
#include <QVector3D>

namespace Game { namespace Systems {

void CommandService::moveUnits(Engine::Core::World& world,
                               const std::vector<Engine::Core::EntityID>& units,
                               const std::vector<QVector3D>& targets) {
    if (units.size() != targets.size()) return;
    for (size_t i = 0; i < units.size(); ++i) {
        auto* e = world.getEntity(units[i]);
        if (!e) continue;
        auto* mv = e->getComponent<Engine::Core::MovementComponent>();
        if (!mv) mv = e->addComponent<Engine::Core::MovementComponent>();
        if (!mv) continue;
        mv->targetX = targets[i].x();
        mv->targetY = targets[i].z();
        mv->hasTarget = true;
    }
}

} } // namespace Game::Systems
