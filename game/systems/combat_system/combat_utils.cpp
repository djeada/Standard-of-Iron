#include "combat_utils.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>

#include "../../core/component.h"
#include "../../core/world.h"
#include "../../core/world_spatial_index.h"
#include "../../units/spawn_type.h"
#include "../building_collision_registry.h"
#include "../combat_rules.h"
#include "../formation_combat_geometry.h"
#include "../nav_grid.h"
#include "../owner_registry.h"
#include "../pathfinding.h"
#include "structure_combat.h"

namespace Game::Systems::Combat {

namespace {

constexpr float k_combat_query_stale_margin = 1.0F;

constexpr int k_bypass_arc_samples = 12;

constexpr float k_min_bypass_standoff = 0.75F;

constexpr float k_bypass_clearance_margin = 0.75F;
} // namespace

CombatQueryContext::CombatQueryContext() {
}

void CombatQueryContext::clear() {
  units.clear();
  world = nullptr;
  nearby_unit_ids.clear();
  m_present_owner_ids.clear();

  if (m_stamp == std::numeric_limits<std::uint32_t>::max()) {
    m_records.clear();
    m_stamp = 0;
  }
  ++m_stamp;
}

void CombatQueryContext::record_candidate(Engine::Core::Entity* entity,
                                          int owner_id,
                                          bool building) {
  if (entity == nullptr) {
    return;
  }
  const Engine::Core::EntityID id = entity->get_id();
  const std::size_t index = Engine::Core::Handle::index_of(id);
  if (index >= m_records.size()) {
    m_records.resize(index + 1U);
  }
  m_records[index] = CandidateRecord{
      .stamp = m_stamp,
      .id = id,
      .entity = entity,
      .owner_id = owner_id,
      .is_building = building,
      .is_wildlife = entity->has_component<Engine::Core::WildlifeComponent>()};

  if (std::find(m_present_owner_ids.begin(), m_present_owner_ids.end(), owner_id) ==
      m_present_owner_ids.end()) {
    m_present_owner_ids.push_back(owner_id);
  }
}

auto CombatQueryContext::find_record(Engine::Core::EntityID entity_id) const
    -> const CandidateRecord* {
  const std::size_t index = Engine::Core::Handle::index_of(entity_id);
  if (index >= m_records.size()) {
    return nullptr;
  }
  const CandidateRecord& record = m_records[index];
  if (record.stamp != m_stamp || record.id != entity_id) {
    return nullptr;
  }
  return &record;
}

auto CombatQueryContext::find_entity(Engine::Core::EntityID entity_id) const
    -> Engine::Core::Entity* {
  const CandidateRecord* record = find_record(entity_id);
  return record != nullptr ? record->entity : nullptr;
}

void CombatQueryContext::rebuild_hostility_table() {
  m_hostility.assign(k_owner_axis * k_owner_axis, 0U);
  const auto& owner_registry = Game::Systems::OwnerRegistry::instance();
  for (const int attacker : m_present_owner_ids) {
    if (attacker < 0 || attacker > k_max_cached_owner_id) {
      continue;
    }
    for (const int target : m_present_owner_ids) {
      if (target < 0 || target > k_max_cached_owner_id) {
        continue;
      }
      const bool is_hostile =
          attacker != target && !owner_registry.are_allies(attacker, target);
      m_hostility[static_cast<std::size_t>(attacker) * k_owner_axis +
                  static_cast<std::size_t>(target)] =
          static_cast<std::uint8_t>(is_hostile ? k_hostility_hostile
                                               : k_hostility_friendly);
    }
  }
}

auto CombatQueryContext::hostile(int attacker_owner_id,
                                 int target_owner_id) const -> bool {
  if (attacker_owner_id >= 0 && attacker_owner_id <= k_max_cached_owner_id &&
      target_owner_id >= 0 && target_owner_id <= k_max_cached_owner_id &&
      !m_hostility.empty()) {
    const std::uint8_t cached =
        m_hostility[static_cast<std::size_t>(attacker_owner_id) * k_owner_axis +
                    static_cast<std::size_t>(target_owner_id)];
    if (cached != k_hostility_unknown) {
      return cached == k_hostility_hostile;
    }
  }
  return attacker_owner_id != target_owner_id &&
         !Game::Systems::OwnerRegistry::instance().are_allies(attacker_owner_id,
                                                              target_owner_id);
}

void collect_unit_ids_near(Engine::Core::World& world,
                           float x,
                           float z,
                           float radius,
                           std::vector<Engine::Core::EntityID>& out) {
  out.clear();
  auto& index = world.spatial_index();
  index.refresh(world);
  index.for_each_in_radius(
      x, z, radius, [&out](const Engine::Core::WorldSpatialIndex::Entry& entry) {
        out.push_back(entry.id);
      });
  std::sort(out.begin(), out.end());
}

auto build_combat_query_context(Engine::Core::World* world) -> CombatQueryContext {
  CombatQueryContext query_context;
  rebuild_combat_query_context(world, query_context);
  return query_context;
}

void rebuild_combat_query_context(Engine::Core::World* world,
                                  CombatQueryContext& query_context) {
  query_context.clear();
  if (world == nullptr) {
    return;
  }

  query_context.units.reserve(
      world->entities_with<Engine::Core::UnitComponent>().size());

  for (auto [entity, unit] : world->entity_view<Engine::Core::UnitComponent>()) {
    if (unit.health <= 0) {
      continue;
    }
    if (entity.has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    const bool building = is_building(&entity);
    query_context.units.push_back(&entity);
    query_context.record_candidate(&entity, unit.owner_id, building);

    if (building) {
      continue;
    }
  }

  query_context.world = world;
  world->spatial_index().refresh(*world);
  query_context.rebuild_hostility_table();
}

auto is_unit_in_hold_mode(Engine::Core::Entity* entity) -> bool {
  if (entity == nullptr) {
    return false;
  }
  auto* hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();
  return (hold_mode != nullptr) && hold_mode->active;
}

auto is_unit_in_guard_mode(Engine::Core::Entity* entity) -> bool {
  if (entity == nullptr) {
    return false;
  }
  auto* guard_mode = entity->get_component<Engine::Core::GuardModeComponent>();
  return (guard_mode != nullptr) && guard_mode->active;
}

auto is_building(Engine::Core::Entity* entity) -> bool {
  if (entity == nullptr) {
    return false;
  }
  return entity->has_component<Engine::Core::BuildingComponent>();
}

auto is_valid_enemy_unit(const Engine::Core::UnitComponent* attacker_unit,
                         Engine::Core::Entity* target,
                         bool allow_buildings) -> bool {
  if (attacker_unit == nullptr) {
    return false;
  }
  return is_valid_enemy_of_owner(attacker_unit->owner_id, target, allow_buildings);
}

auto is_valid_enemy_of_owner(int attacker_owner_id,
                             Engine::Core::Entity* target,
                             bool allow_buildings) -> bool {
  if (target == nullptr) {
    return false;
  }
  if (target->has_component<Engine::Core::PendingRemovalComponent>()) {
    return false;
  }

  auto* target_unit = target->get_component<Engine::Core::UnitComponent>();
  if ((target_unit == nullptr) || (target_unit->health <= 0)) {
    return false;
  }
  if (target_unit->owner_id == attacker_owner_id) {
    return false;
  }

  auto& owner_registry = Game::Systems::OwnerRegistry::instance();
  if (owner_registry.are_allies(attacker_owner_id, target_unit->owner_id)) {
    return false;
  }

  if (!allow_buildings && is_building(target)) {
    return false;
  }

  return true;
}

auto is_passive_wildlife(Engine::Core::Entity* target) -> bool {
  if (target == nullptr) {
    return false;
  }
  const auto* wildlife = target->get_component<Engine::Core::WildlifeComponent>();
  return (wildlife != nullptr) && !wildlife->is_hostile();
}

auto is_auto_acquirable_enemy(const Engine::Core::UnitComponent* attacker_unit,
                              Engine::Core::Entity* target,
                              bool allow_buildings) -> bool {
  if (attacker_unit == nullptr) {
    return false;
  }
  return is_auto_acquirable_enemy_of_owner(
      attacker_unit->owner_id, target, allow_buildings);
}

auto is_auto_acquirable_enemy_of_owner(int attacker_owner_id,
                                       Engine::Core::Entity* target,
                                       bool allow_buildings) -> bool {
  return is_valid_enemy_of_owner(attacker_owner_id, target, allow_buildings) &&
         !is_passive_wildlife(target);
}

auto combat_radius(Engine::Core::Entity* entity) -> float {
  if (entity == nullptr) {
    return 0.0F;
  }

  float radius = 0.0F;
  auto* transform = entity->get_component<Engine::Core::TransformComponent>();
  if (transform != nullptr) {
    radius = std::max(transform->scale.x, transform->scale.z) * 0.5F;
  }

  auto* elephant = entity->get_component<Engine::Core::ElephantComponent>();
  if (elephant != nullptr) {
    radius = std::max(radius, elephant->trample_radius);
  }

  return radius;
}

auto structure_separates_positions(const QVector3D& from, const QVector3D& to) -> bool {
  if (auto* pathfinder = Game::Systems::NavGrid::get_pathfinder()) {
    pathfinder->update_navigation_grid();
    return !pathfinder->is_world_segment_walkable(
        from, to, Game::Systems::Pathfinding::Passability::Light, 0.0F);
  }
  return Game::Systems::BuildingCollisionRegistry::instance()
      .segment_crosses_blocking_building(from.x(), from.z(), to.x(), to.z());
}

constexpr float k_contact_separation_exemption = 1.6F;

auto structure_separates_combatants(Engine::Core::Entity* attacker,
                                    Engine::Core::Entity* target) -> bool {
  if ((attacker == nullptr) || (target == nullptr)) {
    return false;
  }
  if (is_building(attacker) || is_building(target)) {
    return false;
  }

  auto const* attacker_transform =
      attacker->get_component<Engine::Core::TransformComponent>();
  auto const* target_transform =
      target->get_component<Engine::Core::TransformComponent>();
  if ((attacker_transform == nullptr) || (target_transform == nullptr)) {
    return false;
  }

  QVector3D const from(
      attacker_transform->position.x, 0.0F, attacker_transform->position.z);
  QVector3D const to(target_transform->position.x, 0.0F, target_transform->position.z);

  float const separation = (to - from).length();
  if (separation <= k_contact_separation_exemption) {
    return false;
  }
  return structure_separates_positions(from, to);
}

auto melee_bypass_destination(const QVector3D& attacker_position,
                              const QVector3D& target_position,
                              float standoff_distance,
                              float clearance_radius) -> std::optional<QVector3D> {
  QVector3D const target(target_position.x(), 0.0F, target_position.z());
  float const standoff = std::max(standoff_distance, k_min_bypass_standoff);

  QVector3D approach(
      attacker_position.x() - target.x(), 0.0F, attacker_position.z() - target.z());
  float const approach_length_sq = approach.lengthSquared();
  float const base_angle =
      approach_length_sq > 0.000001F ? std::atan2(approach.z(), approach.x()) : 0.0F;

  constexpr float k_arc_step =
      2.0F * std::numbers::pi_v<float> / static_cast<float>(k_bypass_arc_samples);
  for (int index = 0; index < k_bypass_arc_samples; ++index) {
    float const offset = static_cast<float>((index + 1) / 2) * k_arc_step;
    float const angle = base_angle + (index % 2 == 0 ? offset : -offset);
    QVector3D const candidate(target.x() + std::cos(angle) * standoff,
                              0.0F,
                              target.z() + std::sin(angle) * standoff);
    if (structure_separates_positions(candidate, target)) {
      continue;
    }
    if (!Game::Systems::NavGrid::is_world_position_walkable(candidate)) {
      continue;
    }
    float const clearance = clearance_radius + k_bypass_clearance_margin;
    if (Game::Systems::BuildingCollisionRegistry::instance()
            .is_rect_overlapping_blocking_building(candidate.x() - clearance,
                                                   candidate.x() + clearance,
                                                   candidate.z() - clearance,
                                                   candidate.z() + clearance)) {
      continue;
    }
    return candidate;
  }

  return std::nullopt;
}

auto melee_walled_off_from(Engine::Core::Entity* attacker,
                           Engine::Core::Entity* target) -> bool {
  if (attacker == nullptr) {
    return false;
  }
  auto const* attack = attacker->get_component<Engine::Core::AttackComponent>();
  const bool melee_only =
      attack != nullptr &&
      (!attack->can_ranged ||
       attack->preferred_mode == Engine::Core::AttackComponent::CombatMode::Melee);
  return melee_only && structure_separates_combatants(attacker, target);
}

auto is_in_range(Engine::Core::Entity* attacker,
                 Engine::Core::Entity* target,
                 float range) -> bool {
  auto* attacker_transform =
      attacker->get_component<Engine::Core::TransformComponent>();
  auto* target_transform = target->get_component<Engine::Core::TransformComponent>();

  if ((attacker_transform == nullptr) || (target_transform == nullptr)) {
    return false;
  }

  float const dx = target_transform->position.x - attacker_transform->position.x;
  float const dz = target_transform->position.z - attacker_transform->position.z;
  float const dy = target_transform->position.y - attacker_transform->position.y;
  float const distance_squared = dx * dx + dz * dz;

  auto* attacker_atk = attacker->get_component<Engine::Core::AttackComponent>();
  bool const melee =
      (attacker_atk != nullptr) &&
      attacker_atk->current_mode == Engine::Core::AttackComponent::CombatMode::Melee;

  if (!melee && (attacker_atk != nullptr) && attacker_atk->min_range > 0.0F &&
      distance_squared < attacker_atk->min_range * attacker_atk->min_range) {
    return false;
  }

  if (is_building(target)) {
    QVector3D const attacker_position(attacker_transform->position.x,
                                      attacker_transform->position.y,
                                      attacker_transform->position.z);
    if (melee) {
      if (!structure_melee_contact_active(
              *attacker,
              *target,
              Engine::Core::AttackComponent::k_melee_contact_range_grace)) {
        return false;
      }
    } else if (structure_surface_distance(*target, attacker_position) > range) {
      return false;
    }

    if (melee && std::abs(dy) > attacker_atk->max_height_difference) {
      return false;
    }
    return true;
  }

  if (melee && structure_separates_combatants(attacker, target)) {
    return false;
  }

  auto const formation_geometry =
      Game::Systems::FormationCombat::contact_geometry(*attacker, *target);
  if (melee && formation_geometry.uses_formation_slots) {
    if (!Game::Systems::FormationCombat::contact_is_active(
            *attacker, *target, formation_geometry)) {
      return false;
    }
  } else {
    float const effective_range = range + combat_radius(target);

    if (distance_squared > effective_range * effective_range) {
      return false;
    }
  }

  if (melee) {
    float const height_diff = std::abs(dy);
    if (height_diff > attacker_atk->max_height_difference) {
      return false;
    }
  }

  return true;
}

auto suppresses_opportunistic_combat(Engine::Core::Entity* unit) -> bool {
  if (unit == nullptr) {
    return false;
  }

  auto* intent = unit->get_component<Engine::Core::PlayerOrderIntentComponent>();
  auto* movement = unit->get_component<Engine::Core::MovementComponent>();
  return (intent != nullptr) && intent->suppress_opportunistic_combat &&
         (movement != nullptr) &&
         (movement->get_has_target() || movement->has_waypoints());
}

auto may_engage(Engine::Core::Entity* unit,
                Engine::Core::Entity* enemy,
                EngagementTrigger trigger) -> bool {
  if (unit == nullptr || enemy == nullptr || unit == enemy) {
    return false;
  }
  auto* unit_comp = unit->get_component<Engine::Core::UnitComponent>();
  if (unit_comp == nullptr || unit_comp->health <= 0) {
    return false;
  }
  if (unit->has_component<Engine::Core::PendingRemovalComponent>() ||
      enemy->has_component<Engine::Core::PendingRemovalComponent>()) {
    return false;
  }

  if (!auto_acquires_targets(unit)) {
    return false;
  }
  if (!is_auto_acquirable_enemy(unit_comp, enemy, true)) {
    return false;
  }
  if (melee_walled_off_from(unit, enemy)) {
    return false;
  }

  auto const* attack_comp = unit->get_component<Engine::Core::AttackComponent>();
  if (attack_comp != nullptr && attack_comp->in_melee_lock) {
    return false;
  }

  auto* guard_mode = unit->get_component<Engine::Core::GuardModeComponent>();
  if (guard_mode != nullptr && guard_mode->active &&
      guard_mode->returning_to_guard_position) {
    return false;
  }

  if (trigger == EngagementTrigger::Retaliation ||
      trigger == EngagementTrigger::SquadAlert) {

    return true;
  }

  auto* attack_target = unit->get_component<Engine::Core::AttackTargetComponent>();
  if (attack_target != nullptr && attack_target->target_id != 0) {
    return false;
  }
  if (suppresses_opportunistic_combat(unit)) {
    return false;
  }
  auto* movement = unit->get_component<Engine::Core::MovementComponent>();
  return movement == nullptr || !movement->get_has_target();
}

auto is_unit_idle(Engine::Core::Entity* unit) -> bool {
  auto* hold_mode = unit->get_component<Engine::Core::HoldModeComponent>();
  if ((hold_mode != nullptr) && hold_mode->active) {
    return false;
  }

  auto* guard_mode = unit->get_component<Engine::Core::GuardModeComponent>();
  if ((guard_mode != nullptr) && guard_mode->active &&
      guard_mode->returning_to_guard_position) {
    return false;
  }

  auto* attack_target = unit->get_component<Engine::Core::AttackTargetComponent>();
  if ((attack_target != nullptr) && attack_target->target_id != 0) {
    return false;
  }

  auto* movement = unit->get_component<Engine::Core::MovementComponent>();
  if ((movement != nullptr) && movement->get_has_target()) {
    return false;
  }

  auto* attack_comp = unit->get_component<Engine::Core::AttackComponent>();
  if ((attack_comp != nullptr) && attack_comp->in_melee_lock &&
      Game::Systems::CombatRules::participates_in_rts_melee_lock(unit)) {
    return false;
  }

  auto* patrol = unit->get_component<Engine::Core::PatrolComponent>();
  return (patrol == nullptr) || !patrol->patrolling;
}

auto find_nearest_enemy(Engine::Core::Entity* unit,
                        const CombatQueryContext& query_context,
                        float max_range,
                        std::uint64_t* scan_iterations,
                        const TargetFilter& accept) -> Engine::Core::Entity* {
  auto* unit_comp = unit->get_component<Engine::Core::UnitComponent>();
  auto* unit_transform = unit->get_component<Engine::Core::TransformComponent>();
  if ((unit_comp == nullptr) || (unit_transform == nullptr)) {
    return nullptr;
  }

  Engine::Core::Entity* nearest_enemy = nullptr;
  float nearest_dist_sq = max_range * max_range;
  auto& nearby_ids = query_context.nearby_unit_ids;
  nearby_ids.clear();
  if (query_context.world != nullptr) {

    query_context.world->spatial_index().for_each_in_radius(
        unit_transform->position.x,
        unit_transform->position.z,
        max_range + k_combat_query_stale_margin,
        [&nearby_ids](const Engine::Core::WorldSpatialIndex::Entry& entry) {
          if (entry.is(Engine::Core::WorldSpatialIndex::k_building) ||
              entry.is(Engine::Core::WorldSpatialIndex::k_pending_removal) ||
              !entry.is(Engine::Core::WorldSpatialIndex::k_alive)) {
            return;
          }
          nearby_ids.push_back(entry.id);
        });
  }

  const int attacker_owner_id = unit_comp->owner_id;

  for (auto target_id : nearby_ids) {
    if (scan_iterations != nullptr) {
      *scan_iterations += 1;
    }
    const CandidateRecord* record = query_context.find_record(target_id);
    if (record == nullptr || record->is_building) {
      continue;
    }

    auto* target = record->entity;
    if (target == unit) {
      continue;
    }
    if (target->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto* target_unit = target->get_component<Engine::Core::UnitComponent>();
    if ((target_unit == nullptr) || (target_unit->health <= 0)) {
      continue;
    }
    if (!query_context.hostile(attacker_owner_id, target_unit->owner_id)) {
      continue;
    }

    if (record->is_wildlife && is_passive_wildlife(target)) {
      continue;
    }

    auto* target_transform = target->get_component<Engine::Core::TransformComponent>();
    if (target_transform == nullptr) {
      continue;
    }

    float const dx = target_transform->position.x - unit_transform->position.x;
    float const dz = target_transform->position.z - unit_transform->position.z;
    float const dist_sq = dx * dx + dz * dz;

    if (dist_sq >= nearest_dist_sq) {
      continue;
    }
    if (accept && !accept(target)) {
      continue;
    }

    nearest_dist_sq = dist_sq;
    nearest_enemy = target;
  }

  return nearest_enemy;
}

auto combat_role_of(const Engine::Core::Entity* entity) -> Game::Units::CombatRole {
  if (entity == nullptr) {
    return Game::Units::CombatRole::Noncombatant;
  }

  auto const* unit = entity->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return Game::Units::CombatRole::Noncombatant;
  }
  return Game::Units::combat_role(unit->spawn_type);
}

auto auto_acquires_targets(Engine::Core::Entity* entity) -> bool {
  if (!Game::Units::acquires_targets(combat_role_of(entity))) {
    return false;
  }
  if (entity->get_component<Engine::Core::AttackComponent>() == nullptr) {
    return false;
  }

  return Game::Systems::CombatRules::participates_in_rts_melee_lock(entity);
}

auto answers_threat_alerts(Engine::Core::Entity* entity) -> bool {
  if (!Game::Units::answers_threat_alerts(combat_role_of(entity))) {
    return false;
  }

  if (entity->has_component<Engine::Core::CommanderComponent>()) {
    return false;
  }

  return auto_acquires_targets(entity);
}

auto pursues_targets(const Engine::Core::Entity* entity) -> bool {
  return Game::Units::pursues_targets(combat_role_of(entity));
}

auto opens_fire_without_closing(const Engine::Core::Entity* entity) -> bool {
  if (entity == nullptr) {
    return false;
  }
  auto const* attack = entity->get_component<Engine::Core::AttackComponent>();
  return attack != nullptr && attack->can_ranged &&
         attack->preferred_mode != Engine::Core::AttackComponent::CombatMode::Melee;
}

auto acquisition_range(Engine::Core::Entity* entity) -> float {
  auto const* attack = entity->get_component<Engine::Core::AttackComponent>();
  auto const* unit = entity->get_component<Engine::Core::UnitComponent>();
  if (attack == nullptr) {
    return unit != nullptr ? unit->vision_range : 0.0F;
  }

  if (opens_fire_without_closing(entity) || !pursues_targets(entity)) {
    return attack->range;
  }

  return unit != nullptr ? std::max(unit->vision_range, attack->range) : attack->range;
}

} // namespace Game::Systems::Combat
