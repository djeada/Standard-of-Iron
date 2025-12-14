#pragma once

#include "../humanoid_types.h"

namespace Engine::Core {
class Entity;
class World;
class MovementComponent;
class TransformComponent;
class AttackComponent;
class AttackTargetComponent;
class HoldModeComponent;
class PendingRemovalComponent;
class BuildingComponent;
} // namespace Engine::Core

namespace Render::GL {

struct DrawContext;

auto sample_anim_state(const DrawContext &ctx) -> AnimationInputs;

} // namespace Render::GL
