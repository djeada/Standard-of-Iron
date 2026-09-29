#include "army_formation_cohesion.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#include "../core/component_combat.h"
#include "../core/component_gameplay.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../util/planar_math.h"
#include "army_formation_march.h"
#include "army_formation_tuning.h"

namespace Game::Formation::Cohesion {

namespace {

using namespace Tuning;

enum class SlotStanding : std::uint8_t {
  InSlot,
  Busy,
  IdleOffSlot
};

struct ShapeSurvey {
  int expected{0};
  int observed{0};
  int in_slot{0};
  bool all_facing_aligned{true};
  bool controlled_break{false};
  float slowest_speed{std::numeric_limits<float>::max()};
};

void hold_group_facing(Engine::Core::World& world, ArmyFormation& formation) {
  if (!formation.is_formed()) {
    return;
  }
  for (const auto& slot : formation.slot_list) {
    if (slot.occupant == 0U || slot.status == SlotStatus::Blocked) {
      continue;
    }
    const auto* movement =
        world.try_get<Engine::Core::MovementComponent>(slot.occupant);
    if (movement != nullptr && movement->get_has_target()) {
      continue;
    }
    const auto* attack = world.try_get<Engine::Core::AttackComponent>(slot.occupant);
    if (attack != nullptr && attack->in_melee_lock) {
      continue;
    }
    auto* transform = world.try_get<Engine::Core::TransformComponent>(slot.occupant);
    if (transform == nullptr || transform->has_desired_yaw) {
      continue;
    }
    float const drift =
        std::abs(Game::Systems::signed_yaw_delta(slot.facing, transform->rotation.y));
    if (drift <= k_facing_aligned_degrees) {
      continue;
    }
    transform->desired_yaw = slot.facing;
    transform->has_desired_yaw = true;
  }
}

auto slot_standing(Engine::Core::World& world,
                   const FormationSlot& slot,
                   float radius_sq) -> SlotStanding {
  const auto* transform =
      world.try_get<Engine::Core::TransformComponent>(slot.occupant);
  const auto* movement = world.try_get<Engine::Core::MovementComponent>(slot.occupant);
  if (transform == nullptr || movement == nullptr) {
    return SlotStanding::Busy;
  }
  float const off_x = transform->position.x - slot.world_position.x();
  float const off_z = transform->position.z - slot.world_position.z();
  if ((off_x * off_x) + (off_z * off_z) <= radius_sq) {
    return SlotStanding::InSlot;
  }
  const auto* target =
      world.try_get<Engine::Core::AttackTargetComponent>(slot.occupant);
  const auto* attack = world.try_get<Engine::Core::AttackComponent>(slot.occupant);
  const auto* hold = world.try_get<Engine::Core::HoldModeComponent>(slot.occupant);
  const auto* guard = world.try_get<Engine::Core::GuardModeComponent>(slot.occupant);
  if (movement->get_has_target() || (target != nullptr && target->target_id != 0U) ||
      (attack != nullptr && attack->in_melee_lock) ||
      (hold != nullptr && hold->active) || (guard != nullptr && guard->active)) {
    return SlotStanding::Busy;
  }
  return SlotStanding::IdleOffSlot;
}

void track_idle_occupants(Engine::Core::World& world,
                          ArmyFormation& formation,
                          float elapsed) {
  float const radius = formation.spacing * k_in_slot_radius_scale;
  for (const auto& slot : formation.slot_list) {
    if (slot.occupant == 0U || slot.status == SlotStatus::Blocked ||
        !formation.has_member(slot.occupant)) {
      continue;
    }
    switch (slot_standing(world, slot, radius * radius)) {
    case SlotStanding::InSlot:
      formation.straggler_idle.erase(slot.occupant);
      formation.straggler_attempts.erase(slot.occupant);
      break;
    case SlotStanding::Busy:
      formation.straggler_idle.erase(slot.occupant);
      break;
    case SlotStanding::IdleOffSlot:
      formation.straggler_idle[slot.occupant] += elapsed;
      break;
    }
  }
}

void flag_stragglers(ArmyFormation& formation) {
  for (auto const& [member, idle] : formation.straggler_idle) {
    int& attempts = formation.straggler_attempts[member];
    if (idle >= k_straggler_idle_seconds && attempts < k_straggler_max_attempts) {
      ++attempts;
      formation.stragglers.push_back(member);
    }
  }
  if (!formation.stragglers.empty()) {
    for (auto const member : formation.stragglers) {
      formation.straggler_idle.erase(member);
    }
    formation.straggler_cooldown = k_straggler_retry_seconds;
  }
}

auto survey_slots(Engine::Core::World& world,
                  const ArmyFormation& formation) -> ShapeSurvey {
  float const radius = formation.spacing * k_in_slot_radius_scale;
  float const radius_sq = radius * radius;
  ShapeSurvey survey;
  for (const auto& slot : formation.slot_list) {
    if (slot.occupant == 0U || slot.status == SlotStatus::Blocked) {
      continue;
    }
    ++survey.expected;
    auto* entity = world.get_entity(slot.occupant);
    if (entity == nullptr) {
      continue;
    }
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      continue;
    }
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr && unit->speed > 0.0F) {
      survey.slowest_speed = std::min(survey.slowest_speed, unit->speed);
    }
    const auto* movement = entity->get_component<Engine::Core::MovementComponent>();
    survey.controlled_break =
        survey.controlled_break || (movement != nullptr && movement->get_has_target() &&
                                    movement->get_route_lane_scale() < 0.99F);
    ++survey.observed;
    float const off_x = transform->position.x - slot.world_position.x();
    float const off_z = transform->position.z - slot.world_position.z();
    if ((off_x * off_x) + (off_z * off_z) <= radius_sq) {
      ++survey.in_slot;
    }
    survey.all_facing_aligned =
        survey.all_facing_aligned &&
        std::abs(Game::Systems::signed_yaw_delta(transform->rotation.y, slot.facing)) <=
            k_facing_aligned_degrees;
  }
  return survey;
}

void classify_corridor_phase(ArmyFormation& formation,
                             const ShapeSurvey& survey,
                             bool all_in_slot) {
  if (formation.compressed) {
    formation.phase = FormationPhase::Opening;
    return;
  }
  float const opening_distance =
      std::max(formation.spacing, 0.1F) * k_opening_progress_spacing_scale;
  bool const still_opening = survey.controlled_break ||
                             formation.advance_progress < opening_distance ||
                             !all_in_slot || !survey.all_facing_aligned;
  formation.phase =
      still_opening ? FormationPhase::Opening : FormationPhase::Traversing;
}

void classify_destination_phase(ArmyFormation& formation,
                                const ShapeSurvey& survey,
                                bool all_in_slot) {
  if (all_in_slot && survey.all_facing_aligned && March::facing_settled(formation) &&
      !formation.morph.active) {
    formation.has_destination = false;
    formation.phase = FormationPhase::Arrived;
  } else {
    formation.phase = FormationPhase::Reforming;
  }
}

} // namespace

void collect_stragglers(Engine::Core::World& world,
                        ArmyFormation& formation,
                        float elapsed) {
  formation.straggler_cooldown = std::max(0.0F, formation.straggler_cooldown - elapsed);
  if (formation.morph.active || formation.move_plan.has_corridor()) {
    formation.straggler_idle.clear();
    return;
  }
  track_idle_occupants(world, formation, elapsed);
  if (formation.straggler_cooldown > 0.0F || !formation.stragglers.empty()) {
    return;
  }
  flag_stragglers(formation);
}

void refresh_shape_state(Engine::Core::World& world, ArmyFormation& formation) {
  auto const survey = survey_slots(world, formation);

  formation.cohesion_pace =
      formation.maintains_formation() && std::isfinite(survey.slowest_speed)
          ? survey.slowest_speed * k_maintain_speed_multiplier
          : 0.0F;
  if (survey.expected == 0 || survey.observed == 0) {
    formation.cohesion = 0.0F;
    formation.phase = FormationPhase::Disrupted;
    return;
  }

  formation.cohesion =
      static_cast<float>(survey.in_slot) / static_cast<float>(survey.expected);
  bool const all_in_slot = survey.in_slot == survey.expected;

  if (formation.cohesion <= k_disrupted_cohesion) {
    formation.phase = FormationPhase::Disrupted;
    return;
  }
  if (formation.morph.active) {
    formation.phase = formation.cohesion >= k_formed_cohesion && formation.morph.rigid
                          ? FormationPhase::Traversing
                          : FormationPhase::Reforming;
    return;
  }
  if (formation.move_plan.has_corridor()) {
    classify_corridor_phase(formation, survey, all_in_slot);
    return;
  }
  if (formation.has_destination) {
    classify_destination_phase(formation, survey, all_in_slot);
    return;
  }

  formation.phase =
      all_in_slot && formation.phase == FormationPhase::Arrived
          ? FormationPhase::Arrived
          : (all_in_slot ? FormationPhase::Formed : FormationPhase::Reforming);
  hold_group_facing(world, formation);
}

auto damage_taken_multiplier(const ArmyFormation& formation) -> float {
  if (formation.phase == FormationPhase::Disrupted) {
    return k_disrupted_damage_penalty;
  }
  if (!formation.is_formed()) {
    return 1.0F;
  }

  float const span = 1.0F - k_formed_cohesion;
  float const t =
      span <= 0.0F
          ? 1.0F
          : std::clamp((formation.cohesion - k_formed_cohesion) / span, 0.0F, 1.0F);
  return 1.0F + (k_formed_damage_floor - 1.0F) * t;
}

auto move_speed_multiplier(const ArmyFormation& formation,
                           const Engine::Core::Entity& entity) -> float {
  if (!formation.maintains_formation() || formation.morph.active ||
      !formation.move_plan.active) {
    return 1.0F;
  }
  const auto* unit = entity.get_component<Engine::Core::UnitComponent>();
  const auto* transform = entity.get_component<Engine::Core::TransformComponent>();
  if (unit == nullptr || transform == nullptr || unit->speed <= 0.0F) {
    return k_maintain_speed_multiplier;
  }

  float pace = formation.cohesion_pace;
  if (pace <= 0.0F) {
    pace = unit->speed * k_maintain_speed_multiplier;
  }
  QVector3D const position(
      transform->position.x, transform->position.y, transform->position.z);
  float const error = formation.slot_error(position, entity.get_id());
  float const in_slot_radius = formation.spacing * k_in_slot_radius_scale;
  float const recovery_span = std::max(formation.spacing * 4.0F, 0.1F);
  float const recovery =
      error < 0.0F ? 0.0F
                   : std::clamp((error - in_slot_radius) / recovery_span, 0.0F, 0.25F);
  float const target_speed = pace * (1.0F + recovery);

  if (!std::isfinite(target_speed)) {
    return k_maintain_speed_multiplier;
  }
  return std::clamp(target_speed / unit->speed, 0.1F, 1.0F);
}

} // namespace Game::Formation::Cohesion
