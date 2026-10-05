#pragma once

#include "../../core/entity.h"

namespace Engine::Core {
class World;
class Entity;
class TransformComponent;
} // namespace Engine::Core

namespace Game::Systems::Combat {

[[nodiscard]] auto deterministic_attack_delay(Engine::Core::EntityID attacker_id,
                                              Engine::Core::EntityID target_id,
                                              float cooldown) -> float;

void begin_attack_animation(Engine::Core::Entity* attacker, bool preserve_seed = false);

void face_target(Engine::Core::TransformComponent* attacker_transform,
                 Engine::Core::TransformComponent* target_transform);

void stop_unit_movement(Engine::Core::Entity* unit,
                        Engine::Core::TransformComponent* transform);

void drop_attack_target(Engine::Core::World* world, Engine::Core::Entity* attacker);

void clear_orphaned_rts_attack_presentation(Engine::Core::Entity* attacker);

} // namespace Game::Systems::Combat
