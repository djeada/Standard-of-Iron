#pragma once

#include "../../core/component.h"
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

void tick_threat_alerts(Engine::Core::World* world, float delta_time);

} // namespace Game::Systems::Combat
