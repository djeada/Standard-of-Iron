#pragma once

#include <QVector3D>

#include <span>

#include "../../core/entity.h"
#include "combat_action_definition.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::CombatActions {

struct BodyImpactContact {
  Engine::Core::EntityID attacker_id{0};
  Engine::Core::EntityID target_id{0};
  QVector3D contact_point{0.0F, 0.0F, 0.0F};
  float distance{0.0F};
  float local_forward{0.0F};
  float local_right{0.0F};
};

[[nodiscard]] auto find_body_impact_contact(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    const CombatActionDefinition& definition,
    Engine::Core::EntityID target_hint_id = 0,
    std::span<const Engine::Core::EntityID> ignored_target_ids = {})
    -> BodyImpactContact;

} // namespace Game::Systems::CombatActions
