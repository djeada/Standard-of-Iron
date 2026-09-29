#pragma once

#include "command_service.h"
#include "core/component_combat.h"
#include "core/world.h"

namespace Game::Systems {

struct PreparedMove {
  Engine::Core::Entity* entity{nullptr};
  Engine::Core::TransformComponent* transform{nullptr};
  Engine::Core::MovementComponent* movement{nullptr};
  float previous_vx{0.0F};
  float previous_vz{0.0F};
  bool preserve_velocity{false};
};

[[nodiscard]] auto issuer_retargets(MoveOrderKind kind) -> bool;

[[nodiscard]] auto
prepare_move(Engine::Core::World& world,
             Engine::Core::EntityID unit_id,
             const CommandService::MoveOptions& options) -> PreparedMove;

} // namespace Game::Systems
