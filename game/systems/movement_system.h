#pragma once

#include "../../engine/core/system.h"
#include "../../engine/core/world.h"
#include "../../engine/core/component.h"

namespace Game::Systems {

class MovementSystem : public Engine::Core::System {
public:
    void update(Engine::Core::World* world, float deltaTime) override;

private:
    void moveUnit(Engine::Core::Entity* entity, float deltaTime);
    bool hasReachedTarget(const Engine::Core::TransformComponent* transform,
                         const Engine::Core::MovementComponent* movement);
};

} // namespace Game::Systems