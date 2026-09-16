#pragma once

#include <cstdint>

#include "../../core/component_combat.h"
#include "../../core/entity.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::Combat {

auto threat_alert_radius(const Engine::Core::UnitComponent* unit) -> float;

auto has_active_engagement(Engine::Core::World* world,
                           Engine::Core::Entity* entity,
                           const Engine::Core::UnitComponent* unit) -> bool;

void engage_threat_target(Engine::Core::Entity* entity,
                          Engine::Core::EntityID aggressor_id);

auto note_threat(Engine::Core::World* world,
                 Engine::Core::Entity* origin,
                 Engine::Core::Entity* aggressor,
                 Engine::Core::ThreatAlertComponent::Kind kind) -> int;

auto ally_fight_to_join(Engine::Core::World* world,
                        Engine::Core::Entity* unit) -> Engine::Core::Entity*;

enum class AnswerPolicy : std::uint8_t {
  KeepCurrentFight,
  TurnOnAttacker,
};

void answer_attacker(Engine::Core::World* world,
                     Engine::Core::Entity* victim,
                     Engine::Core::Entity* attacker,
                     AnswerPolicy policy);

void tick_threat_alerts(Engine::Core::World* world, float delta_time);

} // namespace Game::Systems::Combat
