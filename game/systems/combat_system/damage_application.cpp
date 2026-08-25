#include "damage_application.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <numbers>
#include <optional>
#include <vector>

#include "../../core/ambient_session.h"
#include "../../core/component.h"
#include "../../core/event_manager.h"
#include "../../core/world.h"
#include "../../formation/army_formation_registry.h"
#include "../../units/spawn_type.h"
#include "../building_collision_registry.h"
#include "../combat_rules.h"
#include "../command_service.h"
#include "../defensive_unit_layout_service.h"
#include "../formation_combat_geometry.h"
#include "../order_service.h"
#include "../wall_network_service.h"
#include "animation/death_pose_manifest.h"
#include "combat_types.h"
#include "combat_utils.h"
#include "structure_combat.h"

namespace Game::Systems::Combat {

namespace {

auto is_mounted_spawn(Game::Units::SpawnType spawn_type) -> bool {
  using Game::Units::SpawnType;
  return spawn_type == SpawnType::MountedKnight ||
         spawn_type == SpawnType::HorseArcher || spawn_type == SpawnType::HorseSpearman;
}

auto resolve_death_profile(const Engine::Core::UnitComponent* unit)
    -> Engine::Core::DeathSequenceProfile {
  using Engine::Core::DeathSequenceProfile;
  if (unit == nullptr) {
    return DeathSequenceProfile::Infantry;
  }
  if (unit->death_sequence_override != 0xFFU &&
      unit->death_sequence_override <=
          static_cast<std::uint8_t>(DeathSequenceProfile::Elephant)) {
    return static_cast<DeathSequenceProfile>(unit->death_sequence_override);
  }
  if (unit->spawn_type == Game::Units::SpawnType::Elephant) {
    return DeathSequenceProfile::Elephant;
  }
  if (is_mounted_spawn(unit->spawn_type)) {
    return DeathSequenceProfile::MountedRider;
  }
  if (Game::Units::is_wildlife_spawn(unit->spawn_type)) {
    return DeathSequenceProfile::Horse;
  }
  return DeathSequenceProfile::Infantry;
}

struct DeathSequenceTiming {
  float state_duration{1.0F};
  float dead_hold_duration{0.8F};
  std::uint8_t sequence_variant{0U};
};

auto infantry_death_variant(Engine::Core::Entity* target,
                            Engine::Core::Entity* attacker,
                            std::uint16_t slot) -> std::uint8_t {
  using Animation::HumanoidDeathCollapse;

  auto const variant_for = [](HumanoidDeathCollapse collapse) -> std::uint8_t {
    for (std::uint8_t v = 0U; v < Animation::k_humanoid_infantry_death_variant_count;
         ++v) {
      if (Animation::humanoid_infantry_death_collapse(v) == collapse) {
        return v;
      }
    }
    return 0U;
  };

  auto const jitter = static_cast<std::uint32_t>(
      (target != nullptr ? target->get_id() * 2654435761U : 0U) + (slot * 40503U));
  bool const flanked_by_jitter = slot != 0U && (jitter >> 13U) % 3U == 0U;

  auto const* target_transform =
      target != nullptr ? target->get_component<Engine::Core::TransformComponent>()
                        : nullptr;
  auto const* attacker_transform =
      attacker != nullptr ? attacker->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
  if (target_transform == nullptr || attacker_transform == nullptr) {
    return variant_for(flanked_by_jitter ? HumanoidDeathCollapse::SideCrumple
                                         : HumanoidDeathCollapse::BackSprawl);
  }

  float const to_attacker_x =
      attacker_transform->position.x - target_transform->position.x;
  float const to_attacker_z =
      attacker_transform->position.z - target_transform->position.z;
  float const length_sq =
      (to_attacker_x * to_attacker_x) + (to_attacker_z * to_attacker_z);
  if (length_sq < 1.0e-4F) {
    return variant_for(HumanoidDeathCollapse::BackSprawl);
  }

  float const yaw = target_transform->rotation.y * std::numbers::pi_v<float> / 180.0F;
  float const facing_dot =
      ((std::sin(yaw) * to_attacker_x) + (std::cos(yaw) * to_attacker_z)) /
      std::sqrt(length_sq);

  if (facing_dot > 0.42F) {
    return variant_for(flanked_by_jitter ? HumanoidDeathCollapse::SideCrumple
                                         : HumanoidDeathCollapse::BackSprawl);
  }
  if (facing_dot < -0.42F) {
    return variant_for(flanked_by_jitter ? HumanoidDeathCollapse::SideCrumple
                                         : HumanoidDeathCollapse::FacePlant);
  }
  return variant_for(HumanoidDeathCollapse::SideCrumple);
}

auto resolve_death_variant(Engine::Core::Entity* target,
                           Engine::Core::Entity* attacker,
                           Engine::Core::DeathSequenceProfile profile,
                           std::uint16_t slot = 0U) -> std::uint8_t {
  switch (profile) {
  case Engine::Core::DeathSequenceProfile::Infantry:
    return infantry_death_variant(target, attacker, slot);
  case Engine::Core::DeathSequenceProfile::MountedRider:
  case Engine::Core::DeathSequenceProfile::Elephant:
  case Engine::Core::DeathSequenceProfile::Horse:
  default:
    return 0U;
  }
}

auto resolve_death_timing(Engine::Core::DeathSequenceProfile profile,
                          std::uint8_t variant) -> DeathSequenceTiming {
  DeathSequenceTiming timing{};
  timing.sequence_variant = variant;
  switch (profile) {
  case Engine::Core::DeathSequenceProfile::MountedRider:
    timing.state_duration = Animation::humanoid_death_collapse_duration(
        Animation::HumanoidDeathCollapse::MountedUnseat);
    timing.dead_hold_duration = 0.95F;
    break;
  case Engine::Core::DeathSequenceProfile::Horse:
    timing.state_duration = 1.20F;
    timing.dead_hold_duration = 1.00F;
    timing.sequence_variant = 0U;
    break;
  case Engine::Core::DeathSequenceProfile::Elephant:
    timing.state_duration = 1.50F;
    timing.dead_hold_duration = 1.25F;
    break;
  case Engine::Core::DeathSequenceProfile::Infantry:
  default:

    timing.state_duration = Animation::humanoid_death_collapse_duration(
        Animation::humanoid_infantry_death_collapse(variant));
    break;
  }
  return timing;
}

void apply_death_sequence(Engine::Core::DeathAnimationComponent& death,
                          Engine::Core::DeathSequenceProfile profile,
                          std::uint8_t variant) {
  auto const timing = resolve_death_timing(profile, variant);
  death.profile = profile;
  death.state = Engine::Core::DeathSequenceState::Dying;
  death.state_time = 0.0F;
  death.state_duration = timing.state_duration;
  death.dead_hold_duration = timing.dead_hold_duration;
  death.sequence_variant = timing.sequence_variant;
}

auto preferred_formation_hit_slot(Engine::Core::Entity* target,
                                  Engine::Core::Entity* attacker)
    -> std::optional<std::uint16_t> {
  if (target == nullptr || attacker == nullptr) {
    return std::nullopt;
  }
  if (!FormationCombat::has_formation_slots(*target)) {
    return std::nullopt;
  }

  auto const* contact =
      attacker->get_component<Engine::Core::FormationContactComponent>();
  if (contact != nullptr) {
    auto const* pairs = &contact->engagement_pairs;
    auto const front =
        std::find_if(contact->fronts.begin(),
                     contact->fronts.end(),
                     [target](auto const& candidate) {
                       return candidate.outgoing && candidate.in_contact &&
                              candidate.opponent_id == target->get_id() &&
                              !candidate.engagement_pairs.empty();
                     });
    if (front != contact->fronts.end()) {
      pairs = &front->engagement_pairs;
    }
    if (auto const selected = FormationCombat::select_damage_engagement_pair(
            *attacker, target->get_id(), *pairs);
        selected.has_value()) {
      return selected->target_slot;
    }
  }

  auto const layout = FormationCombat::resolve_layout(*target);
  auto const* attacker_transform =
      attacker->get_component<Engine::Core::TransformComponent>();
  if (layout.live_slots.empty()) {
    return std::nullopt;
  }
  if (attacker_transform == nullptr) {
    return layout.live_slots.front().index;
  }
  auto const closest = std::min_element(
      layout.live_slots.begin(),
      layout.live_slots.end(),
      [attacker_transform](auto const& lhs, auto const& rhs) {
        float const lhs_dx = lhs.world_x - attacker_transform->position.x;
        float const lhs_dz = lhs.world_z - attacker_transform->position.z;
        float const rhs_dx = rhs.world_x - attacker_transform->position.x;
        float const rhs_dz = rhs.world_z - attacker_transform->position.z;
        return lhs_dx * lhs_dx + lhs_dz * lhs_dz < rhs_dx * rhs_dx + rhs_dz * rhs_dz;
      });
  return closest->index;
}

auto ensure_formation_roster(Engine::Core::Entity& target,
                             int total_count,
                             int expected_live_count)
    -> Engine::Core::FormationRosterPresentationComponent* {
  auto* roster = Engine::Core::get_or_add_component<
      Engine::Core::FormationRosterPresentationComponent>(&target);
  if (roster == nullptr) {
    return nullptr;
  }

  int const current_live_count = static_cast<int>(std::count(
      roster->alive.begin(), roster->alive.end(), static_cast<std::uint8_t>(1U)));
  bool const shaped_for_this_unit =
      roster->total_count == total_count &&
      roster->alive.size() == static_cast<std::size_t>(total_count);
  if (shaped_for_this_unit && current_live_count <= expected_live_count) {

    roster->live_count = static_cast<std::uint16_t>(current_live_count);
    return roster;
  }

  roster->total_count = static_cast<std::uint16_t>(total_count);
  roster->live_count = static_cast<std::uint16_t>(expected_live_count);
  roster->alive.assign(static_cast<std::size_t>(total_count), 0U);
  int const first_live = std::max(0, total_count - expected_live_count);
  for (int slot = first_live; slot < total_count; ++slot) {
    roster->alive[static_cast<std::size_t>(slot)] = 1U;
  }
  ++roster->revision;
  return roster;
}

void publish_formation_hit(
    Engine::Core::Entity& target,
    Engine::Core::EntityID attacker_id,
    std::optional<std::uint16_t> slot,
    Engine::Core::HitReactionKind kind = Engine::Core::HitReactionKind::Flinch,
    Engine::Core::World* world = nullptr) {
  if (!slot.has_value()) {
    return;
  }
  auto* hit = Engine::Core::get_or_add_component<
      Engine::Core::FormationHitPresentationComponent>(&target);
  if (hit == nullptr) {
    return;
  }
  if (std::getenv("SOI_HITDBG") != nullptr) {
    std::fprintf(stderr,
                 "[hitdbg] publish target=%llu slot=%u kind=%d\n",
                 (unsigned long long)target.get_id(),
                 (unsigned)*slot,
                 (int)kind);
  }
  hit->attacker_id = attacker_id;
  hit->soldier_slot = *slot;
  hit->duration = Engine::Core::hit_reaction_duration(kind);
  hit->remaining = hit->duration;
  hit->intensity = kind == Engine::Core::HitReactionKind::Stagger ? 1.2F : 0.85F;
  hit->reaction_kind = kind;
  hit->hit_direction_x = 0.0F;
  hit->hit_direction_z = 0.0F;
  if (world != nullptr && attacker_id != 0) {
    auto const* attacker = world->get_entity(attacker_id);
    auto const* attacker_transform =
        attacker != nullptr
            ? attacker->get_component<Engine::Core::TransformComponent>()
            : nullptr;
    auto const* target_transform =
        target.get_component<Engine::Core::TransformComponent>();
    if (attacker_transform != nullptr && target_transform != nullptr) {
      float const dx = target_transform->position.x - attacker_transform->position.x;
      float const dz = target_transform->position.z - attacker_transform->position.z;
      float const dist = std::hypot(dx, dz);
      if (dist > 0.001F) {
        hit->hit_direction_x = dx / dist;
        hit->hit_direction_z = dz / dist;
      }
    }
  }
  ++hit->revision;
}

auto begin_soldier_casualties(Engine::Core::Entity* target,
                              Engine::Core::Entity* attacker,
                              int prev_health,
                              int new_health,
                              std::optional<std::uint16_t> preferred_slot,
                              const FormationCombat::FormationLayout& previous_layout)
    -> int {
  if (target == nullptr) {
    return 0;
  }

  auto* unit = target->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return 0;
  }

  int const individuals_per_unit =
      FormationCombat::resolve_definition(*unit).total_count;
  if (individuals_per_unit <= 1) {
    return 0;
  }

  int const prev_survivors = Engine::Core::resolve_surviving_individual_count(
      prev_health, unit->max_health, individuals_per_unit);
  int const new_survivors = Engine::Core::resolve_surviving_individual_count(
      new_health, unit->max_health, individuals_per_unit);
  int const previous_front_casualties = individuals_per_unit - prev_survivors;
  int const new_front_casualties = individuals_per_unit - new_survivors;
  if (new_front_casualties <= previous_front_casualties) {
    return 0;
  }

  auto* roster = ensure_formation_roster(*target, individuals_per_unit, prev_survivors);
  auto const profile = resolve_death_profile(unit);
  auto* casualties = Engine::Core::get_or_add_component<
      Engine::Core::SoldierCasualtyAnimationComponent>(target);
  if (casualties == nullptr) {
    return 0;
  }

  auto const spatial_anchors =
      FormationCombat::soldier_spatial_anchors(*target, previous_layout);
  auto spatial_anchor_for_slot =
      [&spatial_anchors](
          std::uint16_t slot) -> const FormationCombat::SoldierSpatialAnchor* {
    auto const found =
        std::find_if(spatial_anchors.begin(),
                     spatial_anchors.end(),
                     [slot](auto const& anchor) { return anchor.slot_index == slot; });
    return found != spatial_anchors.end() ? &*found : nullptr;
  };
  auto next_casualty_slot = [&]() -> std::optional<std::uint16_t> {
    if (roster == nullptr) {
      return std::nullopt;
    }
    if (preferred_slot.has_value() && *preferred_slot < roster->alive.size() &&
        roster->alive[*preferred_slot] != 0U) {
      auto const selected = preferred_slot;
      preferred_slot.reset();
      return selected;
    }
    for (std::size_t slot = 0; slot < roster->alive.size(); ++slot) {
      if (roster->alive[slot] != 0U) {
        return static_cast<std::uint16_t>(slot);
      }
    }
    return std::nullopt;
  };

  int queued_casualties = 0;
  for (int casualty_index = previous_front_casualties;
       casualty_index < new_front_casualties;
       ++casualty_index) {
    auto const selected_slot = next_casualty_slot();
    int const slot =
        selected_slot.has_value() ? static_cast<int>(*selected_slot) : casualty_index;
    Engine::Core::SoldierCasualtyAnimationComponent::Entry entry{};
    entry.slot_index = static_cast<std::uint16_t>(slot);
    if (auto const* soldier =
            spatial_anchor_for_slot(static_cast<std::uint16_t>(slot))) {
      entry.has_local_anchor = true;
      entry.local_x = soldier->local_x;
      entry.local_z = soldier->local_z;
      entry.local_yaw = soldier->local_yaw;
    }
    auto const variant = resolve_death_variant(
        target, attacker, profile, static_cast<std::uint16_t>(slot));
    auto const timing = resolve_death_timing(profile, variant);
    entry.profile = profile;
    entry.state = Engine::Core::DeathSequenceState::Dying;
    entry.state_time = 0.0F;
    entry.state_duration = timing.state_duration;
    entry.dead_hold_duration = timing.dead_hold_duration;
    entry.sequence_variant = timing.sequence_variant;

    auto existing =
        std::find_if(casualties->entries.begin(),
                     casualties->entries.end(),
                     [slot](const auto& active) { return active.slot_index == slot; });
    if (existing != casualties->entries.end()) {
      *existing = entry;
    } else {
      casualties->entries.push_back(entry);
    }
    if (roster != nullptr && slot >= 0 && slot < individuals_per_unit) {
      roster->alive[static_cast<std::size_t>(slot)] = 0U;
      ++roster->revision;
    }
    ++queued_casualties;
  }
  if (roster != nullptr && queued_casualties > 0) {
    roster->live_count = static_cast<std::uint16_t>(std::count(
        roster->alive.begin(), roster->alive.end(), static_cast<std::uint8_t>(1U)));
  }
  return queued_casualties;
}

void fill_formation_front_vacancy(Engine::Core::World* world,
                                  Engine::Core::Entity* casualty) {
  if (world == nullptr || casualty == nullptr) {
    return;
  }
  auto const* vacant = casualty->get_component<Engine::Core::FormationModeComponent>();
  if (vacant == nullptr || !vacant->active || vacant->formation_id == 0 ||
      vacant->stable_rank < 0 || vacant->stable_file < 0) {
    return;
  }
  Engine::Core::Entity* replacement = nullptr;
  int best_rank = std::numeric_limits<int>::max();
  float best_distance_sq = std::numeric_limits<float>::max();
  for (auto [candidate_ref, mode_ref, unit_ref, transform_ref] :
       world->entity_view<Engine::Core::FormationModeComponent,
                          Engine::Core::UnitComponent,
                          Engine::Core::TransformComponent>()) {
    Engine::Core::Entity* candidate = &candidate_ref;
    const auto* mode = &mode_ref;
    const auto* unit = &unit_ref;
    const auto* transform = &transform_ref;
    if (candidate == casualty || unit->health <= 0 ||
        mode->formation_id != vacant->formation_id ||
        mode->stable_file != vacant->stable_file ||
        mode->stable_rank <= vacant->stable_rank) {
      continue;
    }
    float const dx = transform->position.x - vacant->stable_slot_x;
    float const dz = transform->position.z - vacant->stable_slot_z;
    float const distance_sq = dx * dx + dz * dz;
    if (mode->stable_rank < best_rank ||
        (mode->stable_rank == best_rank && distance_sq < best_distance_sq)) {
      replacement = candidate;
      best_rank = mode->stable_rank;
      best_distance_sq = distance_sq;
    }
  }
  if (replacement == nullptr) {
    return;
  }
  auto* replacement_mode =
      replacement->get_component<Engine::Core::FormationModeComponent>();
  replacement_mode->stable_slot_id = vacant->stable_slot_id;
  replacement_mode->stable_rank = vacant->stable_rank;
  replacement_mode->stable_file = vacant->stable_file;
  replacement_mode->stable_slot_x = vacant->stable_slot_x;
  replacement_mode->stable_slot_z = vacant->stable_slot_z;
  CommandService::move_unit(
      *world,
      replacement->get_id(),
      QVector3D(vacant->stable_slot_x, 0.0F, vacant->stable_slot_z),
      {.kind = MoveOrderKind::FormationMove, .preserve_formation_mode = true});
}

void begin_death_sequence(Engine::Core::Entity* target,
                          Engine::Core::Entity* attacker) {
  if (target == nullptr) {
    return;
  }

  auto* unit = target->get_component<Engine::Core::UnitComponent>();
  auto* death =
      Engine::Core::get_or_add_component<Engine::Core::DeathAnimationComponent>(target);
  if (death == nullptr) {
    return;
  }

  auto const profile = target->has_component<Engine::Core::WildlifeComponent>()
                           ? Engine::Core::DeathSequenceProfile::Horse
                           : resolve_death_profile(unit);
  auto const variant = resolve_death_variant(target, attacker, profile);
  apply_death_sequence(*death, profile, variant);
}

void prune_oldest_blood_stain(Engine::Core::World* world) {
  if (world == nullptr) {
    return;
  }

  while (true) {
    const auto blood_stains = world->entities_with<Engine::Core::BloodStainComponent>();
    if (blood_stains.size() <
        static_cast<std::size_t>(Engine::Core::Defaults::k_blood_stain_max_active)) {
      return;
    }

    auto const oldest = std::min_element(blood_stains.begin(), blood_stains.end());
    if (oldest == blood_stains.end()) {
      return;
    }

    world->destroy_entity(*oldest);
  }
}

auto hash01(std::uint32_t value) -> float {
  value ^= value >> 16U;
  value *= 0x7feb352dU;
  value ^= value >> 15U;
  value *= 0x846ca68bU;
  value ^= value >> 16U;
  return static_cast<float>(value & 0x00ffffffU) / static_cast<float>(0x01000000U);
}

auto blood_stain_scale(const Engine::Core::UnitComponent* unit) -> float {
  if (unit == nullptr) {
    return 1.0F;
  }
  if (unit->spawn_type == Game::Units::SpawnType::Elephant) {
    return 1.65F;
  }
  if (is_mounted_spawn(unit->spawn_type)) {
    return 1.25F;
  }
  if (unit->spawn_type == Game::Units::SpawnType::Sheep ||
      unit->spawn_type == Game::Units::SpawnType::Wolf) {

    return 0.42F;
  }
  return 1.0F;
}

auto retaliation_should_chase(Engine::Core::Entity* entity) -> bool {
  if (Game::Systems::DefensiveUnitLayoutService::holds_position(*entity)) {
    return false;
  }
  auto* hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();
  return (hold_mode == nullptr) || !hold_mode->active;
}

auto is_valid_retaliation_attacker(Engine::Core::Entity* attacker) -> bool {
  if (attacker == nullptr) {
    return false;
  }
  auto const* unit = attacker->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return false;
  }
  if (attacker->has_component<Engine::Core::BuildingComponent>()) {
    return unit->spawn_type == Game::Units::SpawnType::DefenseTower;
  }
  return true;
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
  return is_valid_enemy_unit(unit, current, true);
}

auto note_wildlife_aggressor(Engine::Core::Entity* target,
                             Engine::Core::EntityID attacker_id) -> bool {
  auto* wildlife = target->get_component<Engine::Core::WildlifeComponent>();
  if (wildlife == nullptr) {
    return false;
  }
  wildlife->aggressor_id = attacker_id;
  wildlife->hostile_timer = Game::Wildlife::k_hostility_duration;
  wildlife->think_cooldown = 0.0F;
  return true;
}

auto can_retaliate(Engine::Core::Entity* entity,
                   const Engine::Core::UnitComponent* unit) -> bool {
  if ((entity == nullptr) || (unit == nullptr)) {
    return false;
  }
  if (entity->has_component<Engine::Core::BuildingComponent>()) {
    return false;
  }
  if (unit->spawn_type == Game::Units::SpawnType::Civilian) {
    return false;
  }
  if (!Game::Systems::CombatRules::participates_in_rts_melee_lock(entity)) {
    return false;
  }
  return entity->get_component<Engine::Core::AttackComponent>() != nullptr;
}

auto can_reach_attacker(Engine::Core::Entity* entity,
                        Engine::Core::Entity* attacker) -> bool {
  return !melee_walled_off_from(entity, attacker);
}

void engage_retaliation_target(Engine::Core::Entity* entity,
                               Engine::Core::EntityID attacker_id) {
  auto* attack_target =
      Engine::Core::get_or_add_component<Engine::Core::AttackTargetComponent>(entity);
  if (attack_target == nullptr) {
    return;
  }
  attack_target->target_id = attacker_id;
  attack_target->should_chase = retaliation_should_chase(entity);
  attack_target->is_player_command = false;

  if (auto* intent =
          entity->get_component<Engine::Core::PlayerOrderIntentComponent>()) {
    intent->suppress_opportunistic_combat = false;
    intent->kind = Engine::Core::PlayerOrderIntentKind::None;
  }
}

void alert_nearby_allies(Engine::Core::World* world,
                         Engine::Core::Entity* defender,
                         Engine::Core::Entity* attacker) {
  if (world == nullptr) {
    return;
  }
  auto* defender_unit = defender->get_component<Engine::Core::UnitComponent>();
  auto* defender_transform =
      defender->get_component<Engine::Core::TransformComponent>();
  auto* attacker_transform =
      attacker->get_component<Engine::Core::TransformComponent>();
  if ((defender_unit == nullptr) || (defender_transform == nullptr) ||
      (attacker_transform == nullptr)) {
    return;
  }

  float const radius_sq =
      Constants::k_squad_alert_radius * Constants::k_squad_alert_radius;
  int alerted = 0;

  static thread_local std::vector<Engine::Core::EntityID> nearby;
  collect_unit_ids_near(*world,
                        defender_transform->position.x,
                        defender_transform->position.z,
                        Constants::k_squad_alert_radius,
                        nearby);
  for (const Engine::Core::EntityID ally_id : nearby) {
    if (alerted >= Constants::k_max_squad_alert_allies) {
      break;
    }
    auto* ally = world->get_entity(ally_id);
    if ((ally == defender) || (ally == nullptr) ||
        ally->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }
    auto* ally_unit = ally->get_component<Engine::Core::UnitComponent>();
    if ((ally_unit == nullptr) || ally_unit->health <= 0 ||
        ally_unit->owner_id != defender_unit->owner_id) {
      continue;
    }
    auto* ally_transform = ally->get_component<Engine::Core::TransformComponent>();
    if (ally_transform == nullptr) {
      continue;
    }
    float const dx = ally_transform->position.x - defender_transform->position.x;
    float const dz = ally_transform->position.z - defender_transform->position.z;
    if (dx * dx + dz * dz > radius_sq) {
      continue;
    }
    if (!may_engage(ally, attacker, EngagementTrigger::SquadAlert)) {
      continue;
    }
    if (has_active_engagement(world, ally, ally_unit)) {
      continue;
    }
    engage_retaliation_target(ally, attacker->get_id());
    ++alerted;
  }
}

void assign_retaliation_target_if_needed(Engine::Core::World* world,
                                         Engine::Core::Entity* target,
                                         Engine::Core::Entity* attacker) {
  if ((target == nullptr) || (attacker == nullptr) || (world == nullptr)) {
    return;
  }

  if (!is_valid_retaliation_attacker(attacker)) {
    return;
  }

  if (note_wildlife_aggressor(target, attacker->get_id())) {
    return;
  }

  auto* unit = target->get_component<Engine::Core::UnitComponent>();
  if (has_active_engagement(world, target, unit)) {
    return;
  }

  if (may_engage(target, attacker, EngagementTrigger::Retaliation)) {
    engage_retaliation_target(target, attacker->get_id());
  }
  alert_nearby_allies(world, target, attacker);
}

void queue_structure_impact(Engine::Core::Entity& target,
                            const Engine::Core::Entity* attacker,
                            const std::optional<QVector3D>& contact_point) {
  auto const* target_transform =
      target.get_component<Engine::Core::TransformComponent>();
  if (target_transform == nullptr) {
    return;
  }

  QVector3D source(target_transform->position.x,
                   target_transform->position.y,
                   target_transform->position.z - 2.0F);
  if (auto const* attacker_transform =
          attacker != nullptr
              ? attacker->get_component<Engine::Core::TransformComponent>()
              : nullptr;
      attacker_transform != nullptr) {
    source = {attacker_transform->position.x,
              attacker_transform->position.y,
              attacker_transform->position.z};
  }

  auto const profile = structure_attack_profile(attacker);
  auto const surface = closest_structure_surface(target, source);
  QVector3D const point = contact_point.value_or(
      structure_impact_point(target, source, 0.0F, profile.impact_height));

  float radius = 0.38F;
  float intensity = 1.0F;
  float lifetime = 0.75F;
  switch (profile.impact_style) {
  case StructureImpactStyle::LightMelee:
    break;
  case StructureImpactStyle::HeavyMelee:
    radius = 0.48F;
    intensity = 1.25F;
    lifetime = 0.90F;
    break;
  case StructureImpactStyle::Elephant:
    radius = 0.78F;
    intensity = 2.2F;
    lifetime = 1.25F;
    break;
  case StructureImpactStyle::Ballista:
    radius = 0.55F;
    intensity = 1.7F;
    lifetime = 1.0F;
    break;
  case StructureImpactStyle::Catapult:
    radius = 0.95F;
    intensity = 2.6F;
    lifetime = 1.4F;
    break;
  case StructureImpactStyle::Magic:
    radius = 0.68F;
    intensity = 1.9F;
    lifetime = 1.15F;
    break;
  }

  auto* presentation = Engine::Core::get_or_add_component<
      Engine::Core::StructureDamagePresentationComponent>(&target);
  if (presentation == nullptr) {
    return;
  }
  constexpr std::size_t k_max_structure_impacts = 16U;
  if (presentation->impacts.size() >= k_max_structure_impacts) {
    presentation->impacts.erase(presentation->impacts.begin());
  }
  presentation->impacts.push_back({
      .x = point.x(),
      .y = point.y(),
      .z = point.z(),
      .normal_x = surface.outward_normal.x(),
      .normal_z = surface.outward_normal.z(),
      .age = 0.0F,
      .lifetime = lifetime,
      .radius = radius,
      .intensity = intensity,
      .style = static_cast<std::uint8_t>(profile.impact_style),
  });
}

} // namespace

void spawn_blood_stain(Engine::Core::World* world,
                       const Engine::Core::Entity* target,
                       float spread,
                       std::uint32_t variation) {
  if (world == nullptr || target == nullptr) {
    return;
  }

  auto const* transform = target->get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return;
  }

  prune_oldest_blood_stain(world);

  auto* blood_stain = world->create_entity();
  if (blood_stain == nullptr) {
    return;
  }

  auto const* unit = target->get_component<Engine::Core::UnitComponent>();
  auto const id_seed =
      static_cast<std::uint32_t>(target->get_id()) + (variation * 2654435761U);
  auto const position_seed =
      static_cast<std::uint32_t>(std::abs(transform->position.x) * 31.0F +
                                 std::abs(transform->position.z) * 131.0F);
  float const scale = blood_stain_scale(unit);
  float const radius = Engine::Core::Defaults::k_blood_stain_default_radius * scale *
                       (0.85F + hash01(id_seed * 17U + position_seed) * 0.45F);
  float const rotation =
      hash01(id_seed * 97U + position_seed * 3U) * std::numbers::pi_v<float> * 2.0F;
  float const aspect_ratio =
      0.72F + hash01(id_seed * 53U + position_seed * 11U) * 0.62F;
  float const seed = hash01(id_seed * 193U + position_seed * 29U);

  float const offset_angle =
      hash01(id_seed * 311U + position_seed * 7U) * std::numbers::pi_v<float> * 2.0F;
  float const offset_reach =
      spread * std::sqrt(hash01(id_seed * 419U + position_seed * 13U));

  blood_stain->add_component<Engine::Core::TransformComponent>(
      transform->position.x + (std::cos(offset_angle) * offset_reach),
      transform->position.y,
      transform->position.z + (std::sin(offset_angle) * offset_reach));
  blood_stain->add_component<Engine::Core::BloodStainComponent>(
      radius,
      Engine::Core::Defaults::k_blood_stain_default_lifetime,
      rotation,
      aspect_ratio,
      seed);
}

void add_or_extend_stagger(Engine::Core::Entity* entity, float duration) {
  if (entity == nullptr || duration <= 0.0F) {
    return;
  }
  auto* stagger = Engine::Core::get_or_add_component<Engine::Core::StaggerComponent>(
      entity, duration);
  if (stagger != nullptr) {
    stagger->remaining = std::max(stagger->remaining, duration);
  }
}

void add_or_extend_stagger(Engine::Core::Entity* entity,
                           float duration,
                           Engine::Core::StaggerTier tier) {
  if (entity == nullptr || duration <= 0.0F) {
    return;
  }
  auto* stagger = Engine::Core::get_or_add_component<Engine::Core::StaggerComponent>(
      entity, duration);
  if (stagger != nullptr) {
    stagger->remaining = std::max(stagger->remaining, duration);
    if (static_cast<std::uint8_t>(tier) > static_cast<std::uint8_t>(stagger->tier)) {
      stagger->tier = tier;
    }
  }
}

namespace {

[[nodiscard]] auto apply_defensive_unit_layout_damage_scaling(
    const Engine::Core::Entity& target,
    const Engine::Core::Entity* attacker,
    const std::optional<QVector3D>& contact_point,
    int damage) -> int {
  if (damage <= 0) {
    return damage;
  }

  Game::Systems::DefensiveUnitLayoutDamageContext context{};
  if (attacker != nullptr) {
    if (const auto* attacker_unit =
            attacker->get_component<Engine::Core::UnitComponent>()) {
      context.is_cavalry_impact = is_mounted_spawn(attacker_unit->spawn_type);
    }
    if (const auto* attack = attacker->get_component<Engine::Core::AttackComponent>()) {
      context.is_missile =
          attack->current_mode == Engine::Core::AttackComponent::CombatMode::Ranged;
    }
    if (const auto* attacker_transform =
            attacker->get_component<Engine::Core::TransformComponent>()) {
      context.attack_origin = QVector3D(attacker_transform->position.x,
                                        attacker_transform->position.y,
                                        attacker_transform->position.z);
    }
  }
  if (contact_point.has_value() && attacker == nullptr) {
    context.attack_origin = *contact_point;
  }

  float multiplier =
      Game::Systems::DefensiveUnitLayoutService::damage_multiplier(target, context);
  multiplier *= Game::Formation::ArmyFormationRuntime::damage_taken_multiplier(target);
  if (attacker != nullptr) {
    multiplier *=
        Game::Systems::DefensiveUnitLayoutService::attack_output_multiplier(*attacker);
  }

  if (multiplier >= 0.999F && multiplier <= 1.001F) {
    return damage;
  }
  return std::max(
      1, static_cast<int>(std::lround(static_cast<float>(damage) * multiplier)));
}

} // namespace

DamageApplicationResult
apply_unit_damage(Engine::Core::World* world,
                  Engine::Core::Entity* target,
                  int damage,
                  Engine::Core::EntityID attacker_id,
                  std::optional<QVector3D> contact_point,
                  std::optional<std::uint16_t> preferred_soldier_slot,
                  float impact_speed) {
  DamageApplicationResult result;
  if (target == nullptr || damage <= 0) {
    return result;
  }

  auto* unit = target->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return result;
  }

  int attacker_owner_id = 0;
  std::optional<Game::Units::SpawnType> attacker_type_opt;
  Engine::Core::Entity* attacker = nullptr;
  if (attacker_id != 0 && world != nullptr) {
    attacker = world->get_entity(attacker_id);
    if (attacker != nullptr) {
      auto* attacker_unit = attacker->get_component<Engine::Core::UnitComponent>();
      if (attacker_unit != nullptr) {
        attacker_owner_id = attacker_unit->owner_id;
        attacker_type_opt = attacker_unit->spawn_type;
      }
    }
  }

  bool const structure = target->has_component<Engine::Core::BuildingComponent>();
  int const raw_damage =
      structure ? resolve_structure_damage(attacker, damage) : damage;
  int const effective_damage = structure
                                   ? raw_damage
                                   : apply_defensive_unit_layout_damage_scaling(
                                         *target, attacker, contact_point, raw_damage);
  result.previous_health = unit->health;
  result.new_health = result.previous_health;
  if (effective_damage <= 0 || result.previous_health <= 0) {
    return result;
  }
  result.applied_damage = effective_damage;
  result.new_health = std::max(0, result.previous_health - effective_damage);
  result.killed = result.previous_health > 0 && result.new_health <= 0;
  bool const is_killing_blow =
      result.previous_health > 0 && result.previous_health <= effective_damage;
  auto const previous_layout = FormationCombat::resolve_layout(*target);

  unit->health = result.new_health;

  if (preferred_soldier_slot.has_value()) {
    bool const slot_is_live =
        std::any_of(previous_layout.live_slots.begin(),
                    previous_layout.live_slots.end(),
                    [preferred_soldier_slot](auto const& slot) {
                      return slot.index == *preferred_soldier_slot;
                    });
    if (!slot_is_live) {
      preferred_soldier_slot.reset();
    }
  }
  auto const preferred_hit_slot = preferred_soldier_slot.has_value()
                                      ? preferred_soldier_slot
                                      : preferred_formation_hit_slot(target, attacker);
  publish_formation_hit(*target,
                        attacker_id,
                        preferred_hit_slot,
                        Engine::Core::HitReactionKind::Flinch,
                        world);
  result.queued_soldier_casualties = begin_soldier_casualties(target,
                                                              attacker,
                                                              result.previous_health,
                                                              result.new_health,
                                                              preferred_hit_slot,
                                                              previous_layout);
  if (result.queued_soldier_casualties > 0 && !is_killing_blow &&
      !target->has_component<Engine::Core::BuildingComponent>()) {
    spawn_blood_stain(world, target);
  }

  Game::Units::SpawnType const attacker_type =
      attacker_type_opt.value_or(Game::Units::SpawnType::Knight);
  Engine::Core::EventManager::instance().publish(Engine::Core::CombatHitEvent(
      attacker_id, target->get_id(), effective_damage, attacker_type, is_killing_blow));

  if (structure) {
    queue_structure_impact(*target, attacker, contact_point);
  }

  if (unit->health > 0) {
    apply_hit_feedback(target,
                       attacker_id,
                       world,
                       Engine::Core::HitReactionKind::Flinch,
                       {.contact_point = contact_point, .weapon_speed = impact_speed});
    assign_retaliation_target_if_needed(world, target, attacker);
  }

  if (target->has_component<Engine::Core::BuildingComponent>() && unit->health > 0) {
    Engine::Core::EventManager::instance().publish(
        Engine::Core::BuildingAttackedEvent(target->get_id(),
                                            unit->owner_id,
                                            unit->spawn_type,
                                            attacker_id,
                                            attacker_owner_id,
                                            effective_damage));
  }

  if (unit->health <= 0) {
    fill_formation_front_vacancy(world, target);
    int const killer_owner_id = attacker_owner_id;

    Engine::Core::EventManager::instance().publish(
        Engine::Core::UnitDiedEvent(target->get_id(),
                                    unit->owner_id,
                                    unit->spawn_type,
                                    attacker_id,
                                    killer_owner_id));

    auto* target_atk = target->get_component<Engine::Core::AttackComponent>();
    if ((target_atk != nullptr) && target_atk->in_melee_lock &&
        target_atk->melee_lock_target_id != 0) {
      if (world != nullptr) {
        auto* lock_partner = world->get_entity(target_atk->melee_lock_target_id);
        if ((lock_partner != nullptr) &&
            !lock_partner->has_component<Engine::Core::PendingRemovalComponent>()) {
          auto* partner_atk =
              lock_partner->get_component<Engine::Core::AttackComponent>();
          if ((partner_atk != nullptr) &&
              partner_atk->melee_lock_target_id == target->get_id()) {
            partner_atk->in_melee_lock = false;
            partner_atk->melee_lock_target_id = 0;
          }
        }
      }
    }

    if (world != nullptr && target->has_component<Engine::Core::BuildingComponent>()) {
      Game::Session::services_for(*world).building_collision->unregister_building(
          target->get_id());
    }
    if (world != nullptr &&
        target->get_component<Engine::Core::WallSegmentComponent>() != nullptr) {
      WallNetworkService::refresh_world(*world);
    }

    if (auto* movement = target->get_component<Engine::Core::MovementComponent>()) {
      movement->stop();
    }

    Game::Systems::OrderService::exit_hold_mode(target);

    auto* attack = target->get_component<Engine::Core::AttackComponent>();
    if (attack != nullptr) {
      attack->in_melee_lock = false;
      attack->melee_lock_target_id = 0;
    }
    auto* target_selector =
        target->get_component<Engine::Core::AttackTargetComponent>();
    if (target_selector != nullptr) {
      target_selector->target_id = 0;
      target_selector->should_chase = false;
    }

    if (target->has_component<Engine::Core::BuildingComponent>()) {
      if (auto* r = target->get_component<Engine::Core::RenderableComponent>()) {
        r->visible = false;
      }
      target->add_component<Engine::Core::PendingRemovalComponent>();
    } else {
      if (is_killing_blow) {
        spawn_blood_stain(world, target);
      }
      begin_death_sequence(target, attacker);
    }
  }

  return result;
}

void apply_hit_feedback(Engine::Core::Entity* target,
                        Engine::Core::EntityID attacker_id,
                        Engine::Core::World* world) {
  apply_hit_feedback(target, attacker_id, world, Engine::Core::HitReactionKind::Flinch);
}

namespace {

[[nodiscard]] auto
reaction_knockback_scale(Engine::Core::HitReactionKind kind) noexcept -> float {
  switch (kind) {
  case Engine::Core::HitReactionKind::Flinch:
    return 1.0F;
  case Engine::Core::HitReactionKind::Block:
    return 0.55F;
  case Engine::Core::HitReactionKind::Evade:
    return 1.7F;
  case Engine::Core::HitReactionKind::Stagger:
    return 2.4F;
  case Engine::Core::HitReactionKind::Recoil:
    return 0.7F;
  }
  return 1.0F;
}

[[nodiscard]] auto
reaction_pauses_swing(Engine::Core::HitReactionKind kind) noexcept -> bool {
  return kind == Engine::Core::HitReactionKind::Flinch ||
         kind == Engine::Core::HitReactionKind::Stagger;
}

} // namespace

void apply_hit_feedback(Engine::Core::Entity* target,
                        Engine::Core::EntityID attacker_id,
                        Engine::Core::World* world,
                        Engine::Core::HitReactionKind kind,
                        const HitImpulse& impulse) {
  if (target == nullptr) {
    return;
  }

  float const weapon_weight =
      impulse.weapon_speed > 0.0F
          ? std::clamp(impulse.weapon_speed / k_reference_weapon_speed, 0.55F, 2.1F)
          : 1.0F;

  auto* feedback =
      Engine::Core::get_or_add_component<Engine::Core::HitFeedbackComponent>(target);
  if (feedback == nullptr) {
    return;
  }

  feedback->is_reacting = true;
  feedback->recent_damage_remaining =
      Engine::Core::HitFeedbackComponent::k_recent_damage_window;
  feedback->source_attacker_id = attacker_id;
  feedback->reaction_time = 0.0F;
  feedback->reaction_duration = Engine::Core::hit_reaction_duration(kind);
  feedback->reaction_kind = kind;
  feedback->knockback_applied = 0.0F;
  feedback->knockback_x = 0.0F;
  feedback->knockback_z = 0.0F;
  feedback->reaction_intensity = 0.85F;

  auto* target_transform = target->get_component<Engine::Core::TransformComponent>();
  if (target_transform != nullptr && attacker_id != 0 && world != nullptr) {
    auto* attacker = world->get_entity(attacker_id);
    if (attacker != nullptr) {
      auto* attacker_transform =
          attacker->get_component<Engine::Core::TransformComponent>();
      if (attacker_transform != nullptr) {
        auto* attacker_attack =
            attacker->get_component<Engine::Core::AttackComponent>();
        auto* attacker_unit = attacker->get_component<Engine::Core::UnitComponent>();
        float knockback_scale = 1.0F;
        if ((attacker_attack != nullptr) &&
            attacker_attack->current_mode ==
                Engine::Core::AttackComponent::CombatMode::Melee) {
          knockback_scale = 1.25F;
          feedback->reaction_intensity = 1.0F;
        } else {
          knockback_scale = 0.8F;
          feedback->reaction_intensity = 0.70F;
        }
        if (attacker_unit != nullptr &&
            attacker_unit->spawn_type == Game::Units::SpawnType::Elephant) {
          knockback_scale = 2.2F;
          feedback->reaction_intensity = 1.35F;
        }
        knockback_scale *= reaction_knockback_scale(kind) * weapon_weight;
        feedback->reaction_intensity *= weapon_weight;
        if (kind == Engine::Core::HitReactionKind::Stagger) {
          feedback->reaction_intensity = std::max(feedback->reaction_intensity, 1.25F);
        } else if (kind == Engine::Core::HitReactionKind::Recoil) {
          feedback->reaction_intensity = 0.6F;
        }

        bool const from_weapon_contact =
            impulse.contact_point.has_value() && impulse.weapon_speed > 0.0F;
        float const from_x = from_weapon_contact ? impulse.contact_point->x()
                                                 : attacker_transform->position.x;
        float const from_z = from_weapon_contact ? impulse.contact_point->z()
                                                 : attacker_transform->position.z;
        float const dx = target_transform->position.x - from_x;
        float const dz = target_transform->position.z - from_z;
        float const dist = std::sqrt(dx * dx + dz * dz);
        if (dist > 0.001F) {
          feedback->hit_direction_x = dx / dist;
          feedback->hit_direction_z = dz / dist;
          float const knockback = std::clamp(
              Engine::Core::HitFeedbackComponent::k_max_knockback * knockback_scale,
              0.0F,
              Engine::Core::HitFeedbackComponent::k_max_knockback * 2.8F);
          feedback->knockback_x = (dx / dist) * knockback;
          feedback->knockback_z = (dz / dist) * knockback;

          bool const hit_controls_root_facing =
              Game::Systems::CombatRules::uses_rpg_combat_rules(target) ||
              !Game::Systems::FormationCombat::has_formation_slots(*target);
          if (hit_controls_root_facing) {
            float const face_dx =
                attacker_transform->position.x - target_transform->position.x;
            float const face_dz =
                attacker_transform->position.z - target_transform->position.z;
            float const face_dist = std::sqrt(face_dx * face_dx + face_dz * face_dz);
            if (face_dist > 0.001F) {
              float const yaw =
                  std::atan2(face_dx, face_dz) * 180.0F / std::numbers::pi_v<float>;
              target_transform->desired_yaw = yaw;
              target_transform->has_desired_yaw = true;
            }
          }
        }
      }
    }
  }

  auto* combat_state = target->get_component<Engine::Core::CombatStateComponent>();
  if (combat_state != nullptr && reaction_pauses_swing(kind)) {
    combat_state->is_hit_paused = true;
    combat_state->hit_pause_remaining =
        Engine::Core::CombatStateComponent::k_combat_animation_hit_pause_duration;
  }
}

void apply_melee_reaction_feedback(Engine::Core::World* world,
                                   Engine::Core::Entity* target,
                                   Engine::Core::EntityID attacker_id,
                                   Engine::Core::HitReactionKind kind) {
  if (target == nullptr) {
    return;
  }
  auto const* unit = target->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr || unit->health <= 0) {
    return;
  }
  apply_hit_feedback(target, attacker_id, world, kind);
  Engine::Core::Entity* attacker =
      (world != nullptr && attacker_id != 0) ? world->get_entity(attacker_id) : nullptr;
  publish_formation_hit(*target,
                        attacker_id,
                        preferred_formation_hit_slot(target, attacker),
                        kind,
                        world);
}

} // namespace Game::Systems::Combat
