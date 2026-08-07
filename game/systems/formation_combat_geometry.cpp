#include "formation_combat_geometry.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <numbers>
#include <unordered_map>

#include "../core/component.h"
#include "../formation/unit_layout_resolver.h"
#include "../units/spawn_type.h"
#include "../units/troop_config.h"
#include "troop_profile_service.h"

namespace Game::Systems::FormationCombat {
namespace {

constexpr float k_elephant_visual_body_radius = 1.15F;

constexpr float k_elephant_chase_penetration = 1.50F;
constexpr float k_elephant_contact_penetration = 1.10F;

constexpr float k_single_body_reach_share = 0.20F;

constexpr float k_mixed_body_approach_margin = 0.5F;

constexpr float k_single_body_strike_share = 0.55F;

constexpr float k_engagement_close_slack = 0.25F;

auto is_mounted(Game::Units::SpawnType spawn_type) noexcept -> bool {
  using Game::Units::SpawnType;
  return spawn_type == SpawnType::MountedKnight ||
         spawn_type == SpawnType::HorseArcher || spawn_type == SpawnType::HorseSpearman;
}

auto resolve_layout_id(const Engine::Core::UnitComponent& unit,
                       const Game::Formation::FormationDoctrineId& doctrine,
                       Game::Formation::UnitLayoutState state) noexcept
    -> Game::Formation::UnitLayoutId {
  auto const troop_type = Game::Units::spawn_typeToTroopType(unit.spawn_type);
  if (!troop_type.has_value()) {
    return Game::Formation::UnitLayoutLibrary::instance().resolve(
        doctrine, "close_order_infantry");
  }
  return Game::Formation::select_unit_layout(doctrine, *troop_type, state);
}

auto world_slot(const Engine::Core::TransformComponent& transform,
                float local_x,
                float local_z) noexcept -> std::pair<float, float> {
  float const yaw = transform.rotation.y * std::numbers::pi_v<float> / 180.0F;
  float const sin_yaw = std::sin(yaw);
  float const cos_yaw = std::cos(yaw);
  float const world_x = transform.position.x + cos_yaw * local_x + sin_yaw * local_z;
  float const world_z = transform.position.z - sin_yaw * local_x + cos_yaw * local_z;
  return {world_x, world_z};
}

auto melee_reach(const Engine::Core::Entity& entity) noexcept -> float {
  auto const* attack = entity.get_component<Engine::Core::AttackComponent>();
  return attack != nullptr ? std::max(0.0F, attack->melee_range) : 1.5F;
}

auto single_body_radius(const Engine::Core::Entity& entity,
                        const Engine::Core::TransformComponent::Vec3& scale) noexcept
    -> float {
  float radius = std::max(0.05F, std::max(scale.x, scale.z) * 0.5F);
  if (entity.has_component<Engine::Core::ElephantComponent>()) {
    float const visual_scale =
        std::max(0.05F, std::max(std::abs(scale.x), std::abs(scale.z)));
    radius = std::max(radius, k_elephant_visual_body_radius * visual_scale);
  }
  return radius;
}

auto holds_formation_line(const Engine::Core::Entity& entity) noexcept -> bool {
  auto const* attack = entity.get_component<Engine::Core::AttackComponent>();
  if (attack != nullptr && attack->in_melee_lock) {
    return true;
  }
  auto const* contact = entity.get_component<Engine::Core::FormationContactComponent>();
  return contact != nullptr &&
         (contact->in_contact ||
          std::any_of(contact->fronts.begin(),
                      contact->fronts.end(),
                      [](auto const& front) { return front.in_contact; }));
}

auto slot_distance(const SoldierSlot& lhs, const SoldierSlot& rhs) noexcept -> float {
  float const dx = rhs.world_x - lhs.world_x;
  float const dz = rhs.world_z - lhs.world_z;
  return std::sqrt(dx * dx + dz * dz);
}

struct LayoutCacheEntry {
  std::uint64_t local_signature{0};
  float world_x{0.0F};
  float world_z{0.0F};
  float yaw{0.0F};
  float turn_radius{0.0F};
  FormationLayout layout;
};

thread_local std::unordered_map<const Engine::Core::Entity*, LayoutCacheEntry>
    g_layout_cache;

void hash_combine(std::uint64_t& seed, std::uint64_t value) noexcept {
  seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
}

void hash_float(std::uint64_t& seed, float value) noexcept {
  hash_combine(seed, std::bit_cast<std::uint32_t>(value));
}

auto layout_signature(const Engine::Core::Entity& entity) -> std::uint64_t {
  std::uint64_t signature = 0xcbf29ce484222325ULL;
  hash_combine(signature, entity.get_id());
  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  if (unit != nullptr) {
    hash_combine(signature, static_cast<std::uint64_t>(unit->spawn_type));
    hash_combine(signature, static_cast<std::uint64_t>(unit->owner_id));
    hash_combine(signature, static_cast<std::uint64_t>(unit->nation_id));
    hash_combine(
        signature,
        static_cast<std::uint64_t>(unit->render_individuals_per_unit_override));
    hash_combine(signature, unit->uses_nation_formation_profile ? 1U : 0U);
    hash_combine(signature, static_cast<std::uint64_t>(unit->health));
    hash_combine(signature, static_cast<std::uint64_t>(unit->max_health));
  }
  auto const* transform = entity.get_component<Engine::Core::TransformComponent>();
  if (transform != nullptr) {
    hash_float(signature, transform->scale.x);
    hash_float(signature, transform->scale.z);
  }
  hash_combine(signature,
               entity.has_component<Engine::Core::BuildingComponent>() ? 1U : 0U);
  hash_combine(signature,
               entity.has_component<Engine::Core::ElephantComponent>() ? 1U : 0U);
  hash_combine(signature, holds_formation_line(entity) ? 1U : 0U);
  if (auto const* roster =
          entity.get_component<Engine::Core::FormationRosterPresentationComponent>()) {
    hash_combine(signature, static_cast<std::uint64_t>(roster->total_count));
    for (std::uint8_t const alive : roster->alive) {
      hash_combine(signature, alive);
    }
  }
  if (auto const* casualties =
          entity.get_component<Engine::Core::SoldierCasualtyAnimationComponent>()) {
    hash_combine(signature, casualties->entries.size());
    for (auto const& casualty : casualties->entries) {
      hash_combine(signature, casualty.slot_index);
      hash_combine(signature, casualty.has_local_anchor ? 1U : 0U);
      hash_float(signature, casualty.local_x);
      hash_float(signature, casualty.local_z);
      hash_float(signature, casualty.local_yaw);
    }
  }
  return signature;
}

void transform_cached_slots(FormationLayout& layout,
                            const Engine::Core::TransformComponent& transform) {
  auto update = [&transform](std::vector<SoldierSlot>& soldier_slots) {
    for (SoldierSlot& slot : soldier_slots) {
      auto const position = world_slot(transform, slot.local_x, slot.local_z);
      slot.world_x = position.first;
      slot.world_z = position.second;
    }
  };
  update(layout.all_slots);
  update(layout.live_slots);
  update(layout.occupied_slots);
}

void store_layout_cache(const Engine::Core::Entity* entity,
                        std::uint64_t signature,
                        const Engine::Core::TransformComponent& transform,
                        const FormationLayout& layout) {
  if (g_layout_cache.size() > 8192U) {
    g_layout_cache.clear();
  }
  LayoutCacheEntry& cache = g_layout_cache[entity];
  cache.local_signature = signature;
  cache.world_x = transform.position.x;
  cache.world_z = transform.position.z;
  cache.yaw = transform.rotation.y;
  cache.layout = layout;
  cache.turn_radius = 0.0F;
  for (auto const& slot : layout.live_slots) {
    cache.turn_radius =
        std::max(cache.turn_radius, std::hypot(slot.local_x, slot.local_z));
  }
}

} // namespace

auto formation_seed(const Engine::Core::Entity& entity) noexcept -> std::uint32_t {
  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  std::uint32_t seed = entity.get_id() * 0x9E3779B9U;
  if (unit != nullptr) {
    seed ^= static_cast<std::uint32_t>(unit->owner_id) * 0x85EBCA6BU;
  }
  return seed;
}

auto resolve_definition(const Engine::Core::UnitComponent& unit)
    -> FormationDefinition {
  FormationDefinition definition;
  definition.total_count =
      Game::Units::TroopConfig::instance().get_individuals_per_unit(unit.spawn_type);
  definition.max_per_row =
      Game::Units::TroopConfig::instance().get_max_units_per_row(unit.spawn_type);
  definition.spacing =
      Game::Units::TroopConfig::instance().get_formation_spacing(unit.spawn_type);
  definition.doctrine = Game::Formation::default_doctrine_for_nation(unit.nation_id);
  definition.layout =
      resolve_layout_id(unit, definition.doctrine, definition.layout_state);

  if (auto troop_type = Game::Units::spawn_typeToTroopType(unit.spawn_type);
      unit.uses_nation_formation_profile && troop_type) {
    auto const profile =
        TroopProfileService::instance().get_profile(unit.nation_id, *troop_type);
    return resolve_definition(unit, profile);
  }

  if (unit.render_individuals_per_unit_override > 0) {
    definition.total_count = unit.render_individuals_per_unit_override;
  }
  definition.total_count = std::max(1, definition.total_count);
  definition.max_per_row =
      std::clamp(definition.max_per_row, 1, definition.total_count);
  definition.spacing = std::max(0.1F, definition.spacing);
  return definition;
}

auto resolve_definition(const Engine::Core::UnitComponent& unit,
                        const TroopProfile& profile) -> FormationDefinition {
  FormationDefinition definition;
  definition.total_count = profile.individuals_per_unit;
  definition.max_per_row = profile.max_units_per_row;
  definition.spacing = profile.visuals.formation_spacing;
  definition.doctrine = profile.doctrine;
  definition.layout =
      resolve_layout_id(unit, definition.doctrine, definition.layout_state);
  if (unit.render_individuals_per_unit_override > 0) {
    definition.total_count = unit.render_individuals_per_unit_override;
  }
  definition.total_count = std::max(1, definition.total_count);
  definition.max_per_row =
      std::clamp(definition.max_per_row, 1, definition.total_count);
  definition.spacing = std::max(0.1F, definition.spacing);
  return definition;
}

auto living_slot_indices(const Engine::Core::Entity& entity,
                         int total_count) -> std::vector<std::uint16_t> {
  std::vector<std::uint16_t> living;
  if (total_count <= 0) {
    return living;
  }
  living.reserve(static_cast<std::size_t>(total_count));

  auto const* roster =
      entity.get_component<Engine::Core::FormationRosterPresentationComponent>();
  if (roster != nullptr &&
      roster->alive.size() == static_cast<std::size_t>(total_count)) {
    for (int idx = 0; idx < total_count; ++idx) {
      if (roster->alive[static_cast<std::size_t>(idx)] != 0U) {
        living.push_back(static_cast<std::uint16_t>(idx));
      }
    }
    return living;
  }

  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  int const live_count = unit != nullptr
                             ? Engine::Core::resolve_surviving_individual_count(
                                   unit->health, unit->max_health, total_count)
                             : total_count;
  for (int idx = std::max(0, total_count - live_count); idx < total_count; ++idx) {
    living.push_back(static_cast<std::uint16_t>(idx));
  }
  return living;
}

auto resolve_layout(const Engine::Core::Entity& entity) -> FormationLayout {
  FormationLayout result;
  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  auto const* transform = entity.get_component<Engine::Core::TransformComponent>();
  if (unit == nullptr) {
    return result;
  }

  Engine::Core::TransformComponent const identity_transform{};
  auto const& resolved_transform =
      transform != nullptr ? *transform : identity_transform;
  std::uint64_t const signature = layout_signature(entity);
  if (auto cached = g_layout_cache.find(&entity);
      cached != g_layout_cache.end() && cached->second.local_signature == signature) {
    bool const transform_unchanged =
        cached->second.world_x == resolved_transform.position.x &&
        cached->second.world_z == resolved_transform.position.z &&
        cached->second.yaw == resolved_transform.rotation.y;
    if (!transform_unchanged) {
      transform_cached_slots(cached->second.layout, resolved_transform);
      cached->second.world_x = resolved_transform.position.x;
      cached->second.world_z = resolved_transform.position.z;
      cached->second.yaw = resolved_transform.rotation.y;
    }
    return cached->second.layout;
  }

  bool const rigid_body = entity.has_component<Engine::Core::BuildingComponent>() ||
                          entity.has_component<Engine::Core::ElephantComponent>();
  if (rigid_body) {
    result.total_count = 1;
    result.live_count = unit->health > 0 ? 1 : 0;
    result.rows = 1;
    result.cols = 1;
    result.body_radius = std::max(
        0.05F, std::max(resolved_transform.scale.x, resolved_transform.scale.z) * 0.5F);
    if (entity.has_component<Engine::Core::ElephantComponent>()) {
      float const visual_scale =
          std::max(0.05F,
                   std::max(std::abs(resolved_transform.scale.x),
                            std::abs(resolved_transform.scale.z)));
      result.body_radius =
          std::max(result.body_radius, k_elephant_visual_body_radius * visual_scale);
    }
    result.seed = formation_seed(entity);
    if (result.live_count > 0) {
      SoldierSlot const slot{0U,
                             0U,
                             0U,
                             0.0F,
                             0.0F,
                             0.0F,
                             resolved_transform.position.x,
                             resolved_transform.position.z};
      result.all_slots.push_back(slot);
      result.live_slots.push_back(slot);
      result.occupied_slots.push_back(slot);
    }
    store_layout_cache(&entity, signature, resolved_transform, result);
    return result;
  }

  auto const definition = resolve_definition(*unit);
  result.total_count = definition.total_count;
  result.cols = definition.max_per_row;
  result.rows = std::max(1, (result.total_count + result.cols - 1) / result.cols);
  result.spacing = definition.spacing;
  result.body_radius = std::max(
      0.05F, std::max(resolved_transform.scale.x, resolved_transform.scale.z) * 0.5F);
  result.seed = formation_seed(entity);

  result.all_slots.reserve(static_cast<std::size_t>(result.total_count));
  result.live_slots.reserve(static_cast<std::size_t>(result.total_count));
  result.occupied_slots.reserve(static_cast<std::size_t>(result.total_count));
  auto resolve_slot = [&](int stable_idx,
                          int layout_idx,
                          int layout_count,
                          int layout_rows,
                          int layout_cols) {
    auto const slot =
        Game::Formation::rank_slot_for(layout_idx, layout_count, layout_cols);
    int const row = slot.row;
    int const col = slot.col;
    Game::Formation::UnitLayoutQuery query;
    query.layout = definition.layout;
    query.index = layout_idx;
    query.row = row;
    query.col = col;
    query.rows = layout_rows;
    query.cols = layout_cols;
    query.count = layout_count;
    query.spacing = result.spacing;
    query.seed = result.seed;
    auto const offset = Game::Formation::UnitLayoutSystem::instance().offset(query);
    auto const [world_x, world_z] =
        world_slot(resolved_transform, offset.offset_x, offset.offset_z);
    return SoldierSlot{static_cast<std::uint16_t>(stable_idx),
                       static_cast<std::uint16_t>(row),
                       static_cast<std::uint16_t>(col),
                       offset.offset_x,
                       offset.offset_z,
                       offset.yaw_offset,
                       world_x,
                       world_z};
  };
  for (int idx = 0; idx < result.total_count; ++idx) {
    result.all_slots.push_back(
        resolve_slot(idx, idx, result.total_count, result.rows, result.cols));
  }

  std::vector<bool> live_slots(static_cast<std::size_t>(result.total_count), false);
  for (auto const slot : living_slot_indices(entity, result.total_count)) {
    live_slots[static_cast<std::size_t>(slot)] = true;
  }
  result.live_count =
      static_cast<int>(std::count(live_slots.begin(), live_slots.end(), true));

  std::vector<bool> active_casualty_slots(static_cast<std::size_t>(result.total_count),
                                          false);
  auto const* casualties =
      entity.get_component<Engine::Core::SoldierCasualtyAnimationComponent>();
  if (casualties != nullptr) {
    for (auto const& casualty : casualties->entries) {
      int const idx = static_cast<int>(casualty.slot_index);
      if (idx >= 0 && idx < result.total_count) {
        active_casualty_slots[static_cast<std::size_t>(idx)] = true;
      }
    }
  }

  int const compact_cols = std::max(1, std::min(result.cols, result.live_count));
  int const compact_rows =
      std::max(1, (result.live_count + compact_cols - 1) / compact_cols);

  bool const remnant_closes_ranks = result.live_count <= result.cols;
  bool const preserve_stable_slots =
      holds_formation_line(entity) || !remnant_closes_ranks;

  int compact_idx = 0;
  for (int stable_idx = 0; stable_idx < result.total_count; ++stable_idx) {
    if (!live_slots[static_cast<std::size_t>(stable_idx)]) {
      continue;
    }
    int const layout_idx = preserve_stable_slots ? stable_idx : compact_idx;
    int const layout_count =
        preserve_stable_slots ? result.total_count : result.live_count;
    int const layout_rows = preserve_stable_slots ? result.rows : compact_rows;
    int const layout_cols = preserve_stable_slots ? result.cols : compact_cols;
    auto const slot =
        resolve_slot(stable_idx, layout_idx, layout_count, layout_rows, layout_cols);
    result.live_slots.push_back(slot);
    result.occupied_slots.push_back(slot);
    ++compact_idx;
  }

  for (int idx = 0; idx < result.total_count; ++idx) {
    if (live_slots[static_cast<std::size_t>(idx)] ||
        !active_casualty_slots[static_cast<std::size_t>(idx)]) {
      continue;
    }
    SoldierSlot casualty_slot = result.all_slots[static_cast<std::size_t>(idx)];
    if (casualties != nullptr) {
      auto const found =
          std::find_if(casualties->entries.begin(),
                       casualties->entries.end(),
                       [idx](auto const& entry) {
                         return entry.slot_index == static_cast<std::uint16_t>(idx);
                       });
      if (found != casualties->entries.end() && found->has_local_anchor) {
        casualty_slot.local_x = found->local_x;
        casualty_slot.local_z = found->local_z;
        casualty_slot.local_yaw = found->local_yaw;
        auto const [world_x, world_z] =
            world_slot(resolved_transform, found->local_x, found->local_z);
        casualty_slot.world_x = world_x;
        casualty_slot.world_z = world_z;
      }
    }
    result.occupied_slots.push_back(casualty_slot);
  }
  store_layout_cache(&entity, signature, resolved_transform, result);
  return result;
}

auto formation_turn_radius(const Engine::Core::Entity& entity) -> float {
  std::uint64_t const signature = layout_signature(entity);
  auto cached = g_layout_cache.find(&entity);
  if (cached == g_layout_cache.end() || cached->second.local_signature != signature) {
    (void)resolve_layout(entity);
    cached = g_layout_cache.find(&entity);
  }
  return cached != g_layout_cache.end() ? cached->second.turn_radius : 0.0F;
}

auto has_formation_slots(const Engine::Core::Entity& entity) -> bool {
  if (entity.has_component<Engine::Core::BuildingComponent>() ||
      entity.has_component<Engine::Core::ElephantComponent>()) {
    return false;
  }
  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  return unit != nullptr && resolve_definition(*unit).total_count > 1;
}

auto resolve_contact_context(const Engine::Core::Entity& attacker,
                             const Engine::Core::Entity& target)
    -> FormationContactContext {
  FormationContactContext context;
  ContactGeometry& result = context.geometry;
  auto const* attacker_transform =
      attacker.get_component<Engine::Core::TransformComponent>();
  auto const* target_transform =
      target.get_component<Engine::Core::TransformComponent>();
  if (attacker_transform == nullptr || target_transform == nullptr) {
    result.surface_gap = std::numeric_limits<float>::infinity();
    return context;
  }

  float const dx = target_transform->position.x - attacker_transform->position.x;
  float const dz = target_transform->position.z - attacker_transform->position.z;
  result.center_distance = std::sqrt(dx * dx + dz * dz);
  result.uses_formation_slots =
      has_formation_slots(attacker) || has_formation_slots(target);
  if (!result.uses_formation_slots) {
    result.surface_gap = result.center_distance;

    float const body_contact = single_body_radius(attacker, attacker_transform->scale) +
                               single_body_radius(target, target_transform->scale);
    float const reach = melee_reach(attacker);
    result.contact_center_distance = body_contact;
    result.engagement_center_distance =
        body_contact + std::max(0.0F, reach - body_contact) * k_single_body_reach_share;
    return context;
  }

  context.attacker_layout = resolve_layout(attacker);
  context.target_layout = resolve_layout(target);
  auto const& attacker_layout = context.attacker_layout;
  auto const& target_layout = context.target_layout;
  result.formation_overlap_required =
      has_formation_slots(attacker) && has_formation_slots(target);

  result.contact_tolerance =
      std::min(attacker_layout.body_radius, target_layout.body_radius) * 0.15F;
  float nearest = std::numeric_limits<float>::infinity();
  for (auto const& attacker_slot : attacker_layout.occupied_slots) {
    for (auto const& target_slot : target_layout.occupied_slots) {
      nearest = std::min(nearest, slot_distance(attacker_slot, target_slot));
    }
  }
  result.surface_gap =
      nearest - attacker_layout.body_radius - target_layout.body_radius;

  if (result.center_distance > 0.0001F) {
    float const dir_x = dx / result.center_distance;
    float const dir_z = dz / result.center_distance;
    float const contact_radius =
        attacker_layout.body_radius + target_layout.body_radius;
    float const contact_radius_sq = contact_radius * contact_radius;

    for (auto const& attacker_slot : attacker_layout.occupied_slots) {
      float const attacker_offset_x =
          attacker_slot.world_x - attacker_transform->position.x;
      float const attacker_offset_z =
          attacker_slot.world_z - attacker_transform->position.z;
      for (auto const& target_slot : target_layout.occupied_slots) {
        float const relative_x =
            (target_slot.world_x - target_transform->position.x) - attacker_offset_x;
        float const relative_z =
            (target_slot.world_z - target_transform->position.z) - attacker_offset_z;
        float const parallel = relative_x * dir_x + relative_z * dir_z;
        float const relative_sq = relative_x * relative_x + relative_z * relative_z;
        float const lateral_sq = std::max(0.0F, relative_sq - parallel * parallel);
        if (lateral_sq > contact_radius_sq) {
          continue;
        }
        float const candidate = -parallel + std::sqrt(contact_radius_sq - lateral_sq);
        result.contact_center_distance =
            std::max(result.contact_center_distance, candidate);
      }
    }
  }
  if (result.formation_overlap_required) {

    float const rank_spacing = std::min(attacker_layout.spacing, target_layout.spacing);
    float const body_radius =
        std::max(attacker_layout.body_radius, target_layout.body_radius);
    result.engagement_center_distance =
        std::min(result.contact_center_distance,
                 std::max(rank_spacing * 0.30F, body_radius * 0.5F));
  } else if (attacker.has_component<Engine::Core::ElephantComponent>() &&
             has_formation_slots(target)) {

    result.engagement_center_distance = std::max(
        0.0F,
        result.center_distance - (result.surface_gap + k_elephant_chase_penetration));
  } else {

    result.engagement_center_distance =
        std::max(0.0F,
                 result.center_distance - result.surface_gap +
                     melee_reach(attacker) * k_mixed_body_approach_margin);
  }
  return context;
}

auto contact_geometry(const Engine::Core::Entity& attacker,
                      const Engine::Core::Entity& target) -> ContactGeometry {
  return resolve_contact_context(attacker, target).geometry;
}

auto single_combat_strike_distance(const Engine::Core::Entity& attacker,
                                   const Engine::Core::Entity& target,
                                   const ContactGeometry& geometry) -> float {
  (void)target;
  float const contact = geometry.contact_center_distance;
  float const reach = std::max(0.2F, melee_reach(attacker));
  if (contact <= 0.0F) {
    return reach;
  }
  return contact + std::max(0.0F, reach - contact) * k_single_body_strike_share;
}

auto contact_is_active(const Engine::Core::Entity& attacker,
                       const Engine::Core::Entity& target,
                       const ContactGeometry& geometry) -> bool {
  if (!geometry.uses_formation_slots) {
    return false;
  }
  constexpr float k_contact_numeric_epsilon = 0.001F;
  auto const* previous =
      attacker.get_component<Engine::Core::FormationContactComponent>();
  if (previous != nullptr && previous->in_contact &&
      previous->target_id == target.get_id()) {
    return true;
  }
  if (geometry.formation_overlap_required) {

    bool const deep_front_rank_overlap =
        geometry.center_distance <=
        geometry.engagement_center_distance + k_engagement_close_slack;
    auto const* attacker_attack =
        attacker.get_component<Engine::Core::AttackComponent>();
    bool const locked_visible_overlap =
        attacker_attack != nullptr && attacker_attack->in_melee_lock &&
        attacker_attack->melee_lock_target_id == target.get_id() &&
        geometry.surface_gap <= k_contact_numeric_epsilon;

    bool const degenerate_slot_contact =
        geometry.contact_center_distance <= k_contact_numeric_epsilon &&
        geometry.center_distance <= melee_reach(attacker) + k_contact_numeric_epsilon;
    return deep_front_rank_overlap || locked_visible_overlap || degenerate_slot_contact;
  }
  if (attacker.has_component<Engine::Core::ElephantComponent>() &&
      has_formation_slots(target)) {
    return geometry.surface_gap <=
           -k_elephant_contact_penetration + k_contact_numeric_epsilon;
  }

  return geometry.surface_gap <= melee_reach(attacker) + k_contact_numeric_epsilon;
}

auto engaged_soldiers(const Engine::Core::Entity& attacker,
                      const Engine::Core::Entity& target)
    -> std::vector<std::uint16_t> {
  std::vector<std::uint16_t> engaged;
  for (auto const& pair : engagement_pairs(attacker, target)) {
    engaged.push_back(pair.attacker_slot);
  }
  return engaged;
}

auto engagement_pairs(const Engine::Core::Entity& attacker,
                      const Engine::Core::Entity& target)
    -> std::vector<Engine::Core::FormationEngagementPair> {
  auto context = resolve_contact_context(attacker, target);
  return engagement_pairs(
      attacker, target, context.attacker_layout, context.target_layout);
}

auto engagement_pairs(const Engine::Core::Entity& attacker,
                      const Engine::Core::Entity& target,
                      const FormationLayout& attacker_layout,
                      const FormationLayout& target_layout)
    -> std::vector<Engine::Core::FormationEngagementPair> {
  using Pair = Engine::Core::FormationEngagementPair;
  std::vector<Pair> result;
  if (!has_formation_slots(attacker) && !has_formation_slots(target)) {
    return result;
  }
  auto const* attacker_transform =
      attacker.get_component<Engine::Core::TransformComponent>();
  auto const* target_transform =
      target.get_component<Engine::Core::TransformComponent>();
  if (attacker_transform == nullptr || target_transform == nullptr ||
      attacker_layout.live_slots.empty() || target_layout.live_slots.empty()) {
    return result;
  }

  float const contact_distance =
      attacker_layout.body_radius + target_layout.body_radius;
  for (auto const& attacker_slot : attacker_layout.live_slots) {
    auto const* closest_target = &target_layout.live_slots.front();
    float closest_distance = slot_distance(attacker_slot, *closest_target);
    for (auto const& target_slot : target_layout.live_slots) {
      float const distance = slot_distance(attacker_slot, target_slot);
      if (distance < closest_distance ||
          (distance == closest_distance && target_slot.index < closest_target->index)) {
        closest_target = &target_slot;
        closest_distance = distance;
      }
    }
    result.push_back({attacker_slot.index,
                      closest_target->index,
                      closest_distance,
                      closest_distance - contact_distance});
  }
  std::sort(result.begin(), result.end(), [](auto const& lhs, auto const& rhs) {
    return lhs.attacker_slot < rhs.attacker_slot;
  });
  return result;
}

auto select_damage_engagement_pair(
    const Engine::Core::Entity& attacker,
    Engine::Core::EntityID opponent_id,
    const std::vector<Engine::Core::FormationEngagementPair>& pairs)
    -> std::optional<Engine::Core::FormationEngagementPair> {
  if (pairs.empty()) {
    return std::nullopt;
  }

  auto const closest = std::min_element(
      pairs.begin(), pairs.end(), [](auto const& lhs, auto const& rhs) {
        if (lhs.surface_gap != rhs.surface_gap) {
          return lhs.surface_gap < rhs.surface_gap;
        }
        return lhs.attacker_slot < rhs.attacker_slot;
      });
  float const spacing = resolve_layout(attacker).spacing;
  float const equivalent_contact_band = std::max(0.05F, spacing * 0.18F);
  std::vector<const Engine::Core::FormationEngagementPair*> contact_candidates;
  contact_candidates.reserve(pairs.size());
  for (auto const& pair : pairs) {
    if (pair.surface_gap <= closest->surface_gap + equivalent_contact_band) {
      contact_candidates.push_back(&pair);
    }
  }

  std::uint32_t seed = attacker.get_id() * 0x9e3779b9U;
  seed ^= opponent_id * 0x85ebca6bU;
  if (auto const* state =
          attacker.get_component<Engine::Core::CombatStateComponent>()) {
    seed ^= static_cast<std::uint32_t>(state->attack_variant) * 0xc2b2ae35U;
  }
  if (auto const* action =
          attacker.get_component<Engine::Core::RpgCommanderActionComponent>()) {
    seed ^= static_cast<std::uint32_t>(action->combat_action_id) * 0x27d4eb2dU;
    seed ^= static_cast<std::uint32_t>(action->melee_attack_sequence) * 0x165667b1U;
  }
  seed ^= seed >> 16U;
  seed *= 0x7feb352dU;
  seed ^= seed >> 15U;
  return *contact_candidates[seed % contact_candidates.size()];
}

} // namespace Game::Systems::FormationCombat
