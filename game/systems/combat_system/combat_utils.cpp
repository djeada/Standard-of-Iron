#include "combat_utils.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>

#include "../../core/component_economy.h"
#include "../../core/world.h"
#include "../../core/world_spatial_index.h"
#include "../../units/spawn_type.h"
#include "../building_collision_registry.h"
#include "../combat_rules.h"
#include "../command_service.h"
#include "../formation_combat_geometry.h"
#include "../nav_grid.h"
#include "../owner_registry.h"
#include "../pathfinding.h"
#include "structure_combat.h"
#include "target_rules.h"

namespace Game::Systems::Combat {

namespace {

constexpr float k_combat_query_stale_margin = 1.0F;

constexpr int k_bypass_arc_samples = 12;

constexpr float k_min_bypass_standoff = 0.75F;

constexpr float k_bypass_clearance_margin = 0.75F;

constexpr float k_walk_around_arrival_slack = 1.5F;

constexpr float k_max_answer_fire_margin = 12.0F;
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
  m_records[index] = CandidateRecord{.stamp = m_stamp,
                                     .id = id,
                                     .entity = entity,
                                     .owner_id = owner_id,
                                     .is_building = building};

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
      const bool is_hostile = owners_are_hostile(owner_registry, attacker, target);
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
  return owners_are_hostile(
      Game::Systems::OwnerRegistry::instance(), attacker_owner_id, target_owner_id);
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

auto guard_post_of(const Engine::Core::Entity* entity) -> std::optional<QVector3D> {
  auto const* guard = entity == nullptr
                          ? nullptr
                          : entity->get_component<Engine::Core::GuardModeComponent>();
  if ((guard == nullptr) || !guard->active || !guard->has_guard_target) {
    return std::nullopt;
  }
  QVector3D post(guard->guard_position_x, 0.0F, guard->guard_position_z);
  auto const* registry = entity->registry();
  if (guard->guarded_entity_id != 0 && registry != nullptr) {
    if (auto const* guarded = registry->try_get<Engine::Core::TransformComponent>(
            guard->guarded_entity_id)) {
      post = QVector3D(guarded->position.x, 0.0F, guarded->position.z);
    }
  }
  return post;
}

auto guard_reach_of(const Engine::Core::Entity* entity) -> std::optional<GuardReach> {
  auto const post = guard_post_of(entity);
  if (!post.has_value()) {
    return std::nullopt;
  }
  auto const* guard =
      entity->registry()->try_get<Engine::Core::GuardModeComponent>(entity->get_id());
  if (guard->has_reach_center) {
    return GuardReach{
        guard->reach_center_x, guard->reach_center_z, guard->guard_radius};
  }
  return GuardReach{post->x(), post->z(), guard->guard_radius};
}

auto within_guard_reach(const Engine::Core::Entity* entity,
                        float x,
                        float z,
                        float margin) -> bool {
  auto const reach = guard_reach_of(entity);
  if (!reach.has_value()) {
    return true;
  }
  float const dx = x - reach->center_x;
  float const dz = z - reach->center_z;
  float const radius = reach->radius + std::max(0.0F, margin);
  return (dx * dx) + (dz * dz) <= radius * radius;
}

auto guard_answer_fire_margin(const Engine::Core::Entity* aggressor) -> float {
  auto const* attack = aggressor == nullptr
                           ? nullptr
                           : aggressor->get_component<Engine::Core::AttackComponent>();
  if (attack == nullptr) {
    return 0.0F;
  }
  return std::min(std::max(attack->range, attack->melee_range),
                  k_max_answer_fire_margin);
}

auto within_guard_reach(const Engine::Core::Entity* entity,
                        const Engine::Core::Entity* target,
                        GuardReachRule rule) -> bool {
  auto const* transform =
      target == nullptr ? nullptr
                        : target->get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return true;
  }
  float const margin =
      rule == GuardReachRule::AnswersFire ? guard_answer_fire_margin(target) : 0.0F;
  return within_guard_reach(
      entity, transform->position.x, transform->position.z, margin);
}

auto is_returning_to_guard_post(const Engine::Core::Entity* entity) -> bool {
  auto const* guard = entity == nullptr
                          ? nullptr
                          : entity->get_component<Engine::Core::GuardModeComponent>();
  if ((guard == nullptr) || !guard->active || !guard->returning_to_guard_position) {
    return false;
  }
  auto const* movement = entity->get_component<Engine::Core::MovementComponent>();
  return (movement != nullptr) && movement->get_has_target();
}

void send_guard_home(Engine::Core::World& world,
                     Engine::Core::Entity* entity,
                     float arrival_threshold) {
  auto const post = guard_post_of(entity);
  auto const* transform =
      entity == nullptr ? nullptr
                        : entity->get_component<Engine::Core::TransformComponent>();
  if (!post.has_value() || (transform == nullptr)) {
    return;
  }
  auto* guard = entity->get_component<Engine::Core::GuardModeComponent>();
  if (is_returning_to_guard_post(entity)) {
    return;
  }
  guard->returning_to_guard_position = false;

  float const dx = post->x() - transform->position.x;
  float const dz = post->z() - transform->position.z;
  float const threshold = arrival_threshold >= 0.0F
                              ? arrival_threshold
                              : Engine::Core::Defaults::k_guard_return_threshold;
  if ((dx * dx) + (dz * dz) <= threshold * threshold) {
    return;
  }

  guard->returning_to_guard_position = true;
  CommandService::MoveOptions options;
  options.kind = MoveOrderKind::GuardReturn;
  CommandService::move_unit(world, entity->get_id(), *post, options);
}

auto is_building(Engine::Core::Entity* entity) -> bool {
  if (entity == nullptr) {
    return false;
  }
  return entity->has_component<Engine::Core::BuildingComponent>();
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

auto melee_walk_around_length(Engine::Core::Entity* attacker,
                              Engine::Core::Entity* target) -> std::optional<float> {
  if ((attacker == nullptr) || (target == nullptr)) {
    return std::nullopt;
  }
  auto const* attacker_transform =
      attacker->get_component<Engine::Core::TransformComponent>();
  auto const* target_transform =
      target->get_component<Engine::Core::TransformComponent>();
  if ((attacker_transform == nullptr) || (target_transform == nullptr)) {
    return std::nullopt;
  }
  QVector3D const start(
      attacker_transform->position.x, 0.0F, attacker_transform->position.z);
  QVector3D const goal(
      target_transform->position.x, 0.0F, target_transform->position.z);
  auto const geometry = FormationCombat::contact_geometry(*attacker, *target);

  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  if (pathfinder == nullptr) {
    auto const bypass = melee_bypass_destination(
        start,
        goal,
        geometry.contact_center_distance,
        std::max(k_min_bypass_clearance,
                 FormationCombat::formation_navigation_clearance(*attacker)));
    if (!bypass.has_value()) {
      return std::nullopt;
    }
    return (*bypass - start).length() + (goal - *bypass).length();
  }

  pathfinder->update_navigation_grid();
  auto const route = pathfinder->find_path(
      Game::Systems::NavGrid::world_to_grid(attacker_transform->position.x,
                                            attacker_transform->position.z),
      Game::Systems::NavGrid::world_to_grid(target_transform->position.x,
                                            target_transform->position.z),
      Game::Systems::Pathfinding::Passability::Light,
      FormationCombat::formation_navigation_clearance(*attacker));
  if (route.empty()) {
    return std::nullopt;
  }

  QVector3D previous = start;
  float length = 0.0F;
  for (auto const& cell : route) {
    QVector3D const point = Game::Systems::NavGrid::grid_to_world(cell);
    QVector3D const flat(point.x(), 0.0F, point.z());
    length += (flat - previous).length();
    previous = flat;
  }
  float const shortfall = (goal - previous).length();
  if (shortfall > geometry.contact_center_distance + k_walk_around_arrival_slack) {
    return std::nullopt;
  }
  return length + shortfall;
}

auto melee_walled_off_from(Engine::Core::Entity* attacker,
                           Engine::Core::Entity* target,
                           float allowed_detour) -> bool {
  if (attacker == nullptr) {
    return false;
  }
  auto const* attack = attacker->get_component<Engine::Core::AttackComponent>();
  const bool melee_only =
      attack != nullptr &&
      (!attack->can_ranged ||
       attack->preferred_mode == Engine::Core::AttackComponent::CombatMode::Melee);
  if (!melee_only || !structure_separates_combatants(attacker, target)) {
    return false;
  }

  auto const walk = melee_walk_around_length(attacker, target);
  if (!walk.has_value()) {
    return true;
  }
  auto* registry = attacker->registry();
  auto const& from =
      registry->try_get<Engine::Core::TransformComponent>(attacker->get_id())->position;
  auto const& to =
      registry->try_get<Engine::Core::TransformComponent>(target->get_id())->position;
  float const straight = std::hypot(to.x - from.x, to.z - from.z);
  return *walk > straight + allowed_detour;
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
    auto const* registry = unit->registry();
    auto const* own_attack =
        registry == nullptr
            ? nullptr
            : registry->try_get<Engine::Core::AttackComponent>(unit->get_id());
    bool const answers_a_blow_bare_handed =
        trigger == EngagementTrigger::Retaliation &&
        Game::Units::combat_role(unit_comp->spawn_type) ==
            Game::Units::CombatRole::Noncombatant &&
        own_attack != nullptr && own_attack->can_melee &&
        Game::Systems::CombatRules::participates_in_rts_melee_lock(unit);
    if (!answers_a_blow_bare_handed) {
      return false;
    }
  }

  if (!may_attack(
          unit_comp,
          enemy,
          {.intent = EngagementIntent::AutoAcquired, .allow_buildings = true})) {
    return false;
  }
  bool const answering_a_blow = trigger != EngagementTrigger::Opportunity;
  bool const answering_fire = trigger == EngagementTrigger::Retaliation ||
                              trigger == EngagementTrigger::SquadAlert;
  if (melee_walled_off_from(unit,
                            enemy,
                            answering_a_blow ? k_answering_walk_around_detour
                                             : k_opportunity_walk_around_detour)) {
    return false;
  }

  auto const* attack_comp = unit->get_component<Engine::Core::AttackComponent>();
  if (attack_comp != nullptr && attack_comp->in_melee_lock) {
    return false;
  }

  if (!within_guard_reach(unit,
                          enemy,
                          answering_fire ? GuardReachRule::AnswersFire
                                         : GuardReachRule::Strict)) {
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
  return movement == nullptr || !movement->get_has_target() ||
         is_returning_to_guard_post(unit);
}

auto is_unit_idle(Engine::Core::Entity* unit) -> bool {
  auto* hold_mode = unit->get_component<Engine::Core::HoldModeComponent>();
  if ((hold_mode != nullptr) && hold_mode->active) {
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
                        const TargetFilter& accept,
                        Engine::Core::Entity** nearest_considered,
                        TargetQuery query) -> Engine::Core::Entity* {
  auto* unit_comp = unit->get_component<Engine::Core::UnitComponent>();
  auto* unit_transform = unit->get_component<Engine::Core::TransformComponent>();
  if ((unit_comp == nullptr) || (unit_transform == nullptr)) {
    return nullptr;
  }

  Engine::Core::Entity* nearest_enemy = nullptr;
  float nearest_dist_sq = max_range * max_range;
  float considered_dist_sq = nearest_dist_sq;
  if (nearest_considered != nullptr) {
    *nearest_considered = nullptr;
  }
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
    if (record == nullptr) {
      continue;
    }

    auto* target = record->entity;
    if (target == unit) {
      continue;
    }

    auto* target_unit = target->get_component<Engine::Core::UnitComponent>();
    if (target_unit == nullptr) {
      continue;
    }

    if (evaluate_target(target,
                        query_context.hostile(attacker_owner_id, target_unit->owner_id),
                        query) != TargetRefusal::None) {
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
    if (nearest_considered != nullptr && dist_sq < considered_dist_sq) {
      considered_dist_sq = dist_sq;
      *nearest_considered = target;
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
