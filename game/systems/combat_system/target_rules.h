#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace Engine::Core {
class Entity;
class UnitComponent;
class World;
} // namespace Engine::Core

namespace Game::Systems {
class OwnerRegistry;
}

namespace Game::Systems::Combat {

enum class EngagementIntent : std::uint8_t {
  Ordered,
  AutoAcquired
};

enum class TargetRefusal : std::uint8_t {
  None,
  NoTarget,
  SelfOrAllied,
  Passive,
  Structure
};

struct TargetQuery {
  EngagementIntent intent = EngagementIntent::AutoAcquired;
  bool allow_buildings = true;
};

[[nodiscard]] auto evaluate_target(const OwnerRegistry& owners,
                                   int attacker_owner_id,
                                   Engine::Core::Entity* target,
                                   TargetQuery query) -> TargetRefusal;

[[nodiscard]] auto evaluate_target(int attacker_owner_id,
                                   Engine::Core::Entity* target,
                                   TargetQuery query) -> TargetRefusal;

[[nodiscard]] auto evaluate_target(Engine::Core::Entity* target,
                                   bool owners_are_hostile,
                                   TargetQuery query) -> TargetRefusal;

[[nodiscard]] auto
owners_are_hostile(const OwnerRegistry& owners, int owner_a, int owner_b) -> bool;

[[nodiscard]] auto may_attack(const OwnerRegistry& owners,
                              int attacker_owner_id,
                              Engine::Core::Entity* target,
                              TargetQuery query) -> bool;

[[nodiscard]] auto may_attack(int attacker_owner_id,
                              Engine::Core::Entity* target,
                              TargetQuery query) -> bool;

[[nodiscard]] auto may_attack(const Engine::Core::UnitComponent* attacker,
                              Engine::Core::Entity* target,
                              TargetQuery query) -> bool;

[[nodiscard]] auto target_refusal_key(TargetRefusal refusal) -> std::string_view;

[[nodiscard]] auto is_passive_wildlife_target(Engine::Core::Entity* target) -> bool;

[[nodiscard]] auto
collect_hostile_contacts(const Engine::Core::World& world,
                         int owner_id) -> std::vector<Engine::Core::Entity*>;

} // namespace Game::Systems::Combat
