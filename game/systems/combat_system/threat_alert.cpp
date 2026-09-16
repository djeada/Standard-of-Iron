#include "threat_alert.h"

#include <algorithm>
#include <vector>

#include "../../core/component_gameplay.h"
#include "../../core/world.h"
#include "../../units/spawn_type.h"
#include "combat_types.h"
#include "combat_utils.h"
#include "engagement_trace.h"
#include "target_assignment.h"
#include "target_rules.h"

namespace Game::Systems::Combat {

namespace {

auto responder_budget(Engine::Core::ThreatAlertComponent::Kind kind) -> int {
  return kind == Engine::Core::ThreatAlertComponent::Kind::UnderAttack
             ? Constants::k_max_squad_alert_allies
             : Constants::k_sight_responders_per_aggressor;
}

auto answers_alerts(Engine::Core::Entity* ally) -> bool {
  return answers_threat_alerts(ally);
}

auto is_committed_to(Engine::Core::Entity* ally,
                     Engine::Core::EntityID aggressor_id) -> bool {
  auto const* target = ally->get_component<Engine::Core::AttackTargetComponent>();
  return (target != nullptr) && target->target_id == aggressor_id;
}

auto already_sees(const Engine::Core::TransformComponent& ally_transform,
                  const Engine::Core::UnitComponent& ally_unit,
                  const Engine::Core::TransformComponent& enemy_transform) -> bool {
  float const dx = enemy_transform.position.x - ally_transform.position.x;
  float const dz = enemy_transform.position.z - ally_transform.position.z;
  return (dx * dx) + (dz * dz) <= ally_unit.vision_range * ally_unit.vision_range;
}

auto trigger_for(Engine::Core::ThreatAlertComponent::Kind kind) -> EngagementTrigger {
  return kind == Engine::Core::ThreatAlertComponent::Kind::UnderAttack
             ? EngagementTrigger::SquadAlert
             : EngagementTrigger::SightAlert;
}

auto alert_nearby_allies(Engine::Core::World* world,
                         Engine::Core::Entity* origin,
                         Engine::Core::Entity* aggressor,
                         Engine::Core::ThreatAlertComponent::Kind kind) -> int {
  auto* origin_unit = origin->get_component<Engine::Core::UnitComponent>();
  auto* origin_transform = origin->get_component<Engine::Core::TransformComponent>();
  auto* aggressor_transform =
      aggressor->get_component<Engine::Core::TransformComponent>();
  if ((origin_unit == nullptr) || (origin_transform == nullptr) ||
      (aggressor_transform == nullptr)) {
    return 0;
  }

  float const radius = threat_alert_radius(origin_unit);
  float const radius_sq = radius * radius;
  auto const trigger = trigger_for(kind);
  auto const aggressor_id = aggressor->get_id();
  bool const informing_only =
      kind == Engine::Core::ThreatAlertComponent::Kind::EnemySighted;

  static thread_local std::vector<Engine::Core::EntityID> nearby;
  collect_unit_ids_near(*world,
                        origin_transform->position.x,
                        origin_transform->position.z,
                        radius,
                        nearby);

  std::vector<Engine::Core::Entity*> candidates;
  candidates.reserve(nearby.size());
  int committed = !informing_only && is_committed_to(origin, aggressor_id) ? 1 : 0;

  for (const Engine::Core::EntityID ally_id : nearby) {
    auto* ally = world->get_entity(ally_id);
    if ((ally == origin) || (ally == nullptr) ||
        world->has<Engine::Core::PendingRemovalComponent>(ally->get_id())) {
      continue;
    }
    auto* ally_unit = world->try_get<Engine::Core::UnitComponent>(ally->get_id());
    if ((ally_unit == nullptr) || ally_unit->health <= 0 ||
        ally_unit->owner_id != origin_unit->owner_id) {
      continue;
    }
    auto* ally_transform =
        world->try_get<Engine::Core::TransformComponent>(ally->get_id());
    if (ally_transform == nullptr) {
      continue;
    }
    float const dx = ally_transform->position.x - origin_transform->position.x;
    float const dz = ally_transform->position.z - origin_transform->position.z;
    if ((dx * dx) + (dz * dz) > radius_sq) {
      continue;
    }
    if (is_committed_to(ally, aggressor_id)) {
      ++committed;
      continue;
    }
    if (!answers_alerts(ally)) {
      continue;
    }
    if (informing_only &&
        already_sees(*ally_transform, *ally_unit, *aggressor_transform)) {
      continue;
    }
    if (!may_engage(ally, aggressor, trigger)) {
      continue;
    }
    if (has_active_engagement(world, ally, ally_unit)) {
      continue;
    }
    candidates.push_back(ally);
  }

  std::sort(
      candidates.begin(),
      candidates.end(),
      [aggressor_transform](Engine::Core::Entity* lhs, Engine::Core::Entity* rhs) {
        auto const to_aggressor = [aggressor_transform](Engine::Core::Entity* entity) {
          auto const& position =
              entity->get_component<Engine::Core::TransformComponent>()->position;
          float const dx = position.x - aggressor_transform->position.x;
          float const dz = position.z - aggressor_transform->position.z;
          return (dx * dx) + (dz * dz);
        };
        return to_aggressor(lhs) < to_aggressor(rhs);
      });

  int const budget = responder_budget(kind) - committed;
  int alerted = 0;
  for (auto* ally : candidates) {
    if (alerted >= budget || alerted >= Constants::k_max_squad_alert_allies) {
      break;
    }
    engage_threat_target(ally, aggressor_id);
    note_engagement(ally,
                    {.candidate_id = aggressor_id,
                     .target_id = aggressor_id,
                     .acquisition_range = radius,
                     .outcome = EngagementOutcome::AssistedAlly,
                     .source = CommandSource::Auto});
    ++alerted;
  }
  return alerted;
}

} // namespace

auto threat_alert_radius(const Engine::Core::UnitComponent* unit) -> float {
  float const vision =
      unit == nullptr ? Engine::Core::Defaults::k_unit_default_vision_range
                      : std::max(unit->vision_range,
                                 Engine::Core::Defaults::k_unit_default_vision_range);
  return vision * Engine::Core::Defaults::k_vision_reveal_scale;
}

auto has_active_engagement(Engine::Core::World* world,
                           Engine::Core::Entity* entity,
                           const Engine::Core::UnitComponent* unit) -> bool {
  auto* attack = entity->get_component<Engine::Core::AttackComponent>();
  if ((attack != nullptr) && attack->in_melee_lock) {
    return true;
  }
  auto* attack_target = entity->get_component<Engine::Core::AttackTargetComponent>();
  if ((attack_target == nullptr) || attack_target->target_id == 0) {
    return false;
  }
  auto* current = world->get_entity(attack_target->target_id);
  return may_attack(
      unit, current, {.intent = EngagementIntent::Ordered, .allow_buildings = true});
}

void engage_threat_target(Engine::Core::Entity* entity,
                          Engine::Core::EntityID aggressor_id) {
  if (assign_attack_target(entity, aggressor_id, TargetSource::Answering) == nullptr) {
    return;
  }

  if (auto* intent =
          entity->get_component<Engine::Core::PlayerOrderIntentComponent>()) {
    intent->suppress_opportunistic_combat = false;
    intent->kind = Engine::Core::PlayerOrderIntentKind::None;
  }
}

auto note_threat(Engine::Core::World* world,
                 Engine::Core::Entity* origin,
                 Engine::Core::Entity* aggressor,
                 Engine::Core::ThreatAlertComponent::Kind kind) -> int {
  if ((world == nullptr) || (origin == nullptr) || (aggressor == nullptr)) {
    return 0;
  }
  if (origin->has_component<Engine::Core::PendingRemovalComponent>() ||
      aggressor->has_component<Engine::Core::PendingRemovalComponent>()) {
    return 0;
  }
  auto* alert =
      Engine::Core::get_or_add_component<Engine::Core::ThreatAlertComponent>(origin);
  if (alert == nullptr) {
    return 0;
  }
  bool const escalates =
      kind == Engine::Core::ThreatAlertComponent::Kind::UnderAttack &&
      alert->kind == Engine::Core::ThreatAlertComponent::Kind::EnemySighted;
  if (alert->cooldown > 0.0F && !escalates) {
    return 0;
  }
  alert->aggressor_id = aggressor->get_id();
  alert->kind = kind;
  alert->cooldown = Constants::k_threat_alert_interval;
  return alert_nearby_allies(world, origin, aggressor, kind);
}

auto ally_fight_to_join(Engine::Core::World* world,
                        Engine::Core::Entity* unit) -> Engine::Core::Entity* {
  if ((world == nullptr) || (unit == nullptr) || !answers_alerts(unit)) {
    return nullptr;
  }
  auto const* own_unit = unit->get_component<Engine::Core::UnitComponent>();
  auto const* own_transform = unit->get_component<Engine::Core::TransformComponent>();
  if ((own_unit == nullptr) || (own_transform == nullptr)) {
    return nullptr;
  }

  static thread_local std::vector<Engine::Core::EntityID> nearby;
  collect_unit_ids_near(*world,
                        own_transform->position.x,
                        own_transform->position.z,
                        own_unit->vision_range,
                        nearby);

  Engine::Core::Entity* best = nullptr;
  float best_distance_sq = 0.0F;
  for (const Engine::Core::EntityID ally_id : nearby) {
    auto const* ally_unit = world->try_get<Engine::Core::UnitComponent>(ally_id);
    if ((ally_id == unit->get_id()) || (ally_unit == nullptr) ||
        ally_unit->health <= 0 || ally_unit->owner_id != own_unit->owner_id) {
      continue;
    }
    auto const* ally_target =
        world->try_get<Engine::Core::AttackTargetComponent>(ally_id);
    if ((ally_target == nullptr) || ally_target->target_id == 0) {
      continue;
    }
    auto* prey = world->get_entity(ally_target->target_id);
    if ((prey == nullptr) || is_building(prey) || !auto_acquires_targets(prey)) {
      continue;
    }
    auto const* prey_transform =
        prey->get_component<Engine::Core::TransformComponent>();
    if (prey_transform == nullptr) {
      continue;
    }
    float const dx = prey_transform->position.x - own_transform->position.x;
    float const dz = prey_transform->position.z - own_transform->position.z;
    float const distance_sq = (dx * dx) + (dz * dz);
    if (best != nullptr && distance_sq >= best_distance_sq) {
      continue;
    }
    if (!may_engage(unit, prey, EngagementTrigger::SquadAlert)) {
      continue;
    }
    best = prey;
    best_distance_sq = distance_sq;
  }
  return best;
}

void answer_attacker(Engine::Core::World* world,
                     Engine::Core::Entity* victim,
                     Engine::Core::Entity* attacker,
                     AnswerPolicy policy) {
  if ((world == nullptr) || (victim == nullptr) || (attacker == nullptr)) {
    return;
  }
  auto const* unit = victim->get_component<Engine::Core::UnitComponent>();
  bool const keeps_its_fight = policy == AnswerPolicy::KeepCurrentFight &&
                               has_active_engagement(world, victim, unit);
  if (!keeps_its_fight &&
      may_engage(victim, attacker, EngagementTrigger::Retaliation)) {
    engage_threat_target(victim, attacker->get_id());
    note_engagement(victim,
                    {.candidate_id = attacker->get_id(),
                     .target_id = attacker->get_id(),
                     .acquisition_range = 0.0F,
                     .outcome = EngagementOutcome::Retaliated,
                     .source = CommandSource::Auto});
  }
  note_threat(
      world, victim, attacker, Engine::Core::ThreatAlertComponent::Kind::UnderAttack);
}

void tick_threat_alerts(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }
  for (auto [origin_id, alert] : world->view<Engine::Core::ThreatAlertComponent>()) {
    if (alert.cooldown > 0.0F) {
      alert.cooldown = std::max(0.0F, alert.cooldown - delta_time);
    }
  }
}

} // namespace Game::Systems::Combat
