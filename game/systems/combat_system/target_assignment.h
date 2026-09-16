#pragma once

#include <cstdint>

#include "../../core/entity.h"

namespace Engine::Core {
class AttackTargetComponent;
class World;
} // namespace Engine::Core

namespace Game::Systems::Combat {

enum class TargetSource : std::uint8_t {
  Opportunity,
  Answering,
  InReach,
  Patrol,
  MeleeLock,
  Commitment,
};

[[nodiscard]] auto chases_target(const Engine::Core::Entity* unit,
                                 TargetSource source,
                                 bool player_command = false) -> bool;

void set_attack_target(Engine::Core::AttackTargetComponent& attack_target,
                       const Engine::Core::Entity* unit,
                       Engine::Core::EntityID target_id,
                       TargetSource source);

auto assign_attack_target(Engine::Core::Entity* unit,
                          Engine::Core::EntityID target_id,
                          TargetSource source) -> Engine::Core::AttackTargetComponent*;

[[nodiscard]] auto keeps_pursuing(Engine::Core::Entity* attacker,
                                  Engine::Core::Entity* target) -> bool;

} // namespace Game::Systems::Combat
