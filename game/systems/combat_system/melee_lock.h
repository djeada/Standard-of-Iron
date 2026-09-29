#pragma once

#include <unordered_set>

#include "../../core/entity.h"

namespace Engine::Core {
class World;
class Entity;
struct AttackComponent;
struct TransformComponent;
} // namespace Engine::Core

namespace Game::Systems::Combat {

struct CombatQueryContext;

class FacingLedger {
public:
  auto claim(Engine::Core::EntityID id) -> bool { return m_turned.insert(id).second; }

private:
  std::unordered_set<Engine::Core::EntityID> m_turned;
};

void release_structure_lock_for_troop_target(Engine::Core::Entity* attacker,
                                             Engine::Core::AttackComponent* attack_comp,
                                             Engine::Core::World* world);

void process_melee_lock(Engine::Core::Entity* attacker,
                        Engine::Core::AttackComponent* attack_comp,
                        Engine::Core::World* world,
                        float delta_time,
                        FacingLedger& ledger);

[[nodiscard]] auto
locked_target_for_attack(Engine::Core::Entity* attacker,
                         Engine::Core::AttackComponent* attack_comp,
                         Engine::Core::World* world) -> Engine::Core::Entity*;

void sync_melee_lock_target(Engine::Core::Entity* attacker,
                            Engine::Core::AttackComponent* attack_comp);

void drop_target_left_by_a_finished_lock(
    Engine::Core::World* world,
    Engine::Core::Entity* attacker,
    const Engine::Core::AttackComponent* attack_comp);

[[nodiscard]] auto
bodies_have_met(Engine::Core::Entity& attacker,
                const Engine::Core::TransformComponent& attacker_transform,
                Engine::Core::Entity& target,
                const Engine::Core::TransformComponent& target_transform) -> bool;

auto enter_melee_lock(Engine::Core::Entity* attacker,
                      Engine::Core::Entity* target,
                      Engine::Core::AttackComponent* attack_comp,
                      Engine::Core::World* world,
                      float delta_time,
                      FacingLedger& ledger) -> bool;

void initiate_melee_combat(Engine::Core::Entity* attacker,
                           Engine::Core::Entity* target,
                           Engine::Core::AttackComponent* attack_comp,
                           Engine::Core::World* world,
                           float delta_time,
                           FacingLedger& ledger);

void lock_touching_enemies(Engine::Core::World* world,
                           const CombatQueryContext& query_context,
                           float delta_time,
                           FacingLedger& ledger);

} // namespace Game::Systems::Combat
