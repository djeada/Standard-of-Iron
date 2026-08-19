#pragma once

#include <QVector3D>

#include <optional>

#include "../../core/component.h"
#include "../../core/entity.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::Combat {

struct DamageApplicationResult {
  int previous_health{0};
  int new_health{0};
  int applied_damage{0};
  bool killed{false};
  int queued_soldier_casualties{0};
};

DamageApplicationResult
apply_unit_damage(Engine::Core::World* world,
                  Engine::Core::Entity* target,
                  int damage,
                  Engine::Core::EntityID attacker_id = 0,
                  std::optional<QVector3D> contact_point = std::nullopt,
                  std::optional<std::uint16_t> preferred_soldier_slot = std::nullopt);

void apply_hit_feedback(Engine::Core::Entity* target,
                        Engine::Core::EntityID attacker_id,
                        Engine::Core::World* world);

void apply_hit_feedback(Engine::Core::Entity* target,
                        Engine::Core::EntityID attacker_id,
                        Engine::Core::World* world,
                        Engine::Core::HitReactionKind kind);

void apply_melee_reaction_feedback(Engine::Core::World* world,
                                   Engine::Core::Entity* target,
                                   Engine::Core::EntityID attacker_id,
                                   Engine::Core::HitReactionKind kind);

void spawn_blood_stain(Engine::Core::World* world,
                       const Engine::Core::Entity* target,
                       float spread = 0.0F,
                       std::uint32_t variation = 0U);

void add_or_extend_stagger(Engine::Core::Entity* entity, float duration);

void add_or_extend_stagger(Engine::Core::Entity* entity,
                           float duration,
                           Engine::Core::StaggerTier tier);

} // namespace Game::Systems::Combat
