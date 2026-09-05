#include "formation_combat_geometry.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../core/component.h"
#include "../formation/traversal_layout_policy.h"
#include "../formation/unit_layout_resolver.h"
#include "../units/spawn_type.h"
#include "../units/troop_catalog.h"
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

thread_local std::uint64_t g_layout_cache_generation = 0;

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
    hash_combine(signature, static_cast<std::uint64_t>(unit->squad_strength));
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
    ++g_layout_cache_generation;
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

constexpr float k_max_slot_body_radius = 3.0F;

auto max_contact_extent() -> float {

  static const float extent = [] {
    float widest = 0.0F;
    for (const auto& [_, troop_class] :
         Game::Units::TroopCatalog::instance().get_all_classes()) {
      const float row_width =
          static_cast<float>(std::max(1, troop_class.max_units_per_row)) *
          std::max(0.1F, troop_class.visuals.formation_spacing);
      widest = std::max(widest, row_width);
    }

    return widest * 0.5F + k_max_slot_body_radius;
  }();
  return extent;
}

auto resolve_definition(const Engine::Core::UnitComponent& unit)
    -> FormationDefinition {
  if (auto troop_type = Game::Units::spawn_typeToTroopType(unit.spawn_type);
      unit.uses_nation_formation_profile && troop_type) {
    auto const& profile =
        TroopProfileService::instance().get_profile_ref(unit.nation_id, *troop_type);
    return resolve_definition(unit, profile);
  }

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

  if (unit.render_individuals_per_unit_override > 0) {
    definition.total_count = unit.render_individuals_per_unit_override;
  } else if (unit.squad_strength > 0) {

    definition.total_count = std::min(definition.total_count, unit.squad_strength);
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
  } else if (unit.squad_strength > 0) {

    definition.total_count = std::min(definition.total_count, unit.squad_strength);
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

  int const live_count = living_slot_count(entity, total_count);
  for (int idx = std::max(0, total_count - live_count); idx < total_count; ++idx) {
    living.push_back(static_cast<std::uint16_t>(idx));
  }
  return living;
}

auto living_slot_count(const Engine::Core::Entity& entity, int total_count) -> int {
  if (total_count <= 0) {
    return 0;
  }

  auto const* roster =
      entity.get_component<Engine::Core::FormationRosterPresentationComponent>();
  if (roster != nullptr &&
      roster->alive.size() == static_cast<std::size_t>(total_count)) {
    return static_cast<int>(std::count_if(
        roster->alive.begin(), roster->alive.end(), [](std::uint8_t alive) {
          return alive != 0U;
        }));
  }

  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return total_count;
  }
  return Engine::Core::resolve_surviving_individual_count(
      unit->health, unit->max_health, total_count);
}

namespace {

void build_layout_into_cache(const Engine::Core::Entity& entity,
                             const Engine::Core::UnitComponent& unit_ref,
                             const Engine::Core::TransformComponent& resolved_transform,
                             std::uint64_t signature);

auto resolve_layout_entry(const Engine::Core::Entity& entity,
                          std::uint64_t signature) -> const LayoutCacheEntry*;

auto resolve_layout_entry(const Engine::Core::Entity& entity,
                          std::uint64_t signature) -> const LayoutCacheEntry* {
  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return nullptr;
  }
  auto const* transform = entity.get_component<Engine::Core::TransformComponent>();

  Engine::Core::TransformComponent const identity_transform{};
  auto const& resolved_transform =
      transform != nullptr ? *transform : identity_transform;
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
    return &cached->second;
  }

  build_layout_into_cache(entity, *unit, resolved_transform, signature);
  auto const rebuilt = g_layout_cache.find(&entity);
  return rebuilt != g_layout_cache.end() ? &rebuilt->second : nullptr;
}

auto resolve_layout_entry(const Engine::Core::Entity& entity)
    -> const LayoutCacheEntry* {
  return resolve_layout_entry(entity, layout_signature(entity));
}

void build_layout_into_cache(const Engine::Core::Entity& entity,
                             const Engine::Core::UnitComponent& unit_ref,
                             const Engine::Core::TransformComponent& resolved_transform,
                             std::uint64_t signature) {
  FormationLayout result;
  auto const* unit = &unit_ref;

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
    return;
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
}

} // namespace

auto resolve_layout(const Engine::Core::Entity& entity) -> FormationLayout {
  auto const* entry = resolve_layout_entry(entity);
  return entry != nullptr ? entry->layout : FormationLayout{};
}

void soldier_spatial_anchors_into(const Engine::Core::Entity& entity,
                                  const FormationLayout& base_layout,
                                  std::vector<SoldierSpatialAnchor>& result) {
  result.clear();
  result.reserve(base_layout.live_slots.size());
  auto const* transform = entity.get_component<Engine::Core::TransformComponent>();
  auto const* traversal =
      entity.get_component<Engine::Core::UnitTraversalLayoutStateComponent>();
  auto const* presentation =
      entity.get_component<Engine::Core::FormationPresentationComponent>();
  float const root_x = transform != nullptr ? transform->position.x : 0.0F;
  float const root_z = transform != nullptr ? transform->position.z : 0.0F;
  float const yaw = transform != nullptr
                        ? transform->rotation.y * std::numbers::pi_v<float> / 180.0F
                        : 0.0F;
  float const sin_yaw = std::sin(yaw);
  float const cos_yaw = std::cos(yaw);
  constexpr std::size_t k_missing_soldier = std::numeric_limits<std::size_t>::max();
  thread_local std::vector<std::size_t> presentation_by_slot;
  presentation_by_slot.clear();
  if (presentation != nullptr) {
    std::uint16_t max_slot = 0U;
    for (auto const& base : base_layout.live_slots) {
      max_slot = std::max(max_slot, base.index);
    }
    presentation_by_slot.assign(static_cast<std::size_t>(max_slot) + 1U,
                                k_missing_soldier);
    for (std::size_t index = 0; index < presentation->soldiers.size(); ++index) {
      auto const& soldier = presentation->soldiers[index];
      if (soldier.alive && soldier.slot_index < presentation_by_slot.size()) {
        presentation_by_slot[soldier.slot_index] = index;
      }
    }
  }

  thread_local std::vector<const Engine::Core::UnitTraversalSlotState*>
      traversal_by_slot;
  traversal_by_slot.clear();
  if (traversal != nullptr) {
    for (auto const& slot : traversal->slot_states) {
      if (slot.slot_index >= traversal_by_slot.size()) {
        traversal_by_slot.resize(static_cast<std::size_t>(slot.slot_index) + 1U,
                                 nullptr);
      }
      if (traversal_by_slot[slot.slot_index] == nullptr) {
        traversal_by_slot[slot.slot_index] = &slot;
      }
    }
  }

  auto traversal_slot_for =
      [&](std::uint16_t slot_index) -> const Engine::Core::UnitTraversalSlotState* {
    if (traversal == nullptr) {
      return nullptr;
    }
    if (slot_index < traversal->slot_states.size() &&
        traversal->slot_states[slot_index].slot_index == slot_index) {
      return &traversal->slot_states[slot_index];
    }
    return slot_index < traversal_by_slot.size() ? traversal_by_slot[slot_index]
                                                 : nullptr;
  };

  for (auto const& base : base_layout.live_slots) {
    SoldierSpatialAnchor anchor{
        .slot_index = base.index,
        .row = base.row,
        .col = base.col,
        .local_x = base.local_x,
        .local_z = base.local_z,
        .local_yaw = base.local_yaw,
        .source = SoldierAnchorSource::BaseLayout,
    };
    if (traversal != nullptr) {
      if (auto const* slot = traversal_slot_for(base.index);
          slot != nullptr && slot->alive) {
        anchor.row = slot->row;
        anchor.col = slot->col;
        anchor.local_x = slot->current_local_x;
        anchor.local_z = slot->current_local_z;
        anchor.source = SoldierAnchorSource::TraversalLayout;
      }
    }
    if (presentation != nullptr && base.index < presentation_by_slot.size()) {
      std::size_t const soldier_index = presentation_by_slot[base.index];
      if (soldier_index != k_missing_soldier) {
        auto const& soldier = presentation->soldiers[soldier_index];
        anchor.row = soldier.row;
        anchor.col = soldier.col;
        anchor.local_x = soldier.local_x;
        anchor.local_z = soldier.local_z;
        anchor.local_yaw = soldier.local_yaw;
        anchor.source = SoldierAnchorSource::PresentationFacts;
      }
    }
    anchor.world_x = root_x + cos_yaw * anchor.local_x + sin_yaw * anchor.local_z;
    anchor.world_z = root_z - sin_yaw * anchor.local_x + cos_yaw * anchor.local_z;
    result.push_back(anchor);
  }
}

auto soldier_spatial_anchors(const Engine::Core::Entity& entity,
                             const FormationLayout& base_layout)
    -> std::vector<SoldierSpatialAnchor> {
  std::vector<SoldierSpatialAnchor> result;
  soldier_spatial_anchors_into(entity, base_layout, result);
  return result;
}

auto soldier_spatial_anchors(const Engine::Core::Entity& entity)
    -> std::vector<SoldierSpatialAnchor> {
  return soldier_spatial_anchors(entity, resolve_layout(entity));
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

auto formation_navigation_clearance(const Engine::Core::Entity& entity) -> float {
  auto const layout = resolve_layout(entity);
  float lateral_extent = layout.body_radius;
  float minimum_z = 0.0F;
  float maximum_z = 0.0F;
  for (auto const& slot : layout.live_slots) {
    lateral_extent =
        std::max(lateral_extent, std::abs(slot.local_x) + layout.body_radius);
    minimum_z = std::min(minimum_z, slot.local_z - layout.body_radius);
    maximum_z = std::max(maximum_z, slot.local_z + layout.body_radius);
  }
  if (layout.live_slots.size() < 20U) {
    return std::max(0.1F, lateral_extent);
  }

  float const compact_spacing = Game::Formation::TraversalPolicy::compact_spacing(
      layout.body_radius, layout.spacing);
  float const single_file_depth =
      layout.body_radius * 2.0F +
      static_cast<float>(layout.live_slots.size() - 1U) * compact_spacing;
  float const normal_depth = std::max(layout.spacing, maximum_z - minimum_z);
  float const depth_ratio = single_file_depth / normal_depth;
  float const depth_premium = std::clamp((depth_ratio - 2.0F) * 0.12F, 0.0F, 0.75F);
  return std::max(0.1F, lateral_extent * (1.0F + depth_premium));
}

auto has_formation_slots(const Engine::Core::Entity& entity) -> bool {
  if (entity.has_component<Engine::Core::BuildingComponent>() ||
      entity.has_component<Engine::Core::ElephantComponent>()) {
    return false;
  }
  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  return unit != nullptr && resolve_definition(*unit).total_count > 1;
}

namespace {

void spatialize_layout_into(const Engine::Core::Entity& entity,
                            const FormationLayout& base_layout,
                            FormationLayout& result) {
  result = base_layout;
  thread_local std::vector<SoldierSpatialAnchor> anchors;
  soldier_spatial_anchors_into(entity, base_layout, anchors);
  thread_local std::vector<const SoldierSpatialAnchor*> anchor_by_slot;
  anchor_by_slot.clear();
  for (auto const& anchor : anchors) {
    if (anchor.slot_index >= anchor_by_slot.size()) {
      anchor_by_slot.resize(static_cast<std::size_t>(anchor.slot_index) + 1U, nullptr);
    }
    anchor_by_slot[anchor.slot_index] = &anchor;
  }
  auto apply = [](std::vector<SoldierSlot>& slot_list) {
    for (auto& slot : slot_list) {
      if (slot.index >= anchor_by_slot.size() ||
          anchor_by_slot[slot.index] == nullptr) {
        continue;
      }
      auto const& anchor = *anchor_by_slot[slot.index];
      slot.row = anchor.row;
      slot.col = anchor.col;
      slot.local_x = anchor.local_x;
      slot.local_z = anchor.local_z;
      slot.local_yaw = anchor.local_yaw;
      slot.world_x = anchor.world_x;
      slot.world_z = anchor.world_z;
    }
  };
  apply(result.all_slots);
  apply(result.live_slots);
  apply(result.occupied_slots);
}

struct LayoutRevisions {
  std::uint32_t traversal{0U};
  std::uint32_t presentation{0U};
  bool has_traversal{false};
  bool has_presentation{false};

  auto operator==(const LayoutRevisions&) const -> bool = default;
};

auto layout_revisions_of(const Engine::Core::Entity& entity) noexcept
    -> LayoutRevisions {
  auto const* traversal =
      entity.get_component<Engine::Core::UnitTraversalLayoutStateComponent>();
  auto const* presentation =
      entity.get_component<Engine::Core::FormationPresentationComponent>();
  return {.traversal = traversal != nullptr ? traversal->slot_states_revision : 0U,
          .presentation = presentation != nullptr ? presentation->revision : 0U,
          .has_traversal = traversal != nullptr,
          .has_presentation = presentation != nullptr};
}

struct SpatialLayoutCacheEntry {
  std::uint64_t base_signature{0};
  float world_x{0.0F};
  float world_z{0.0F};
  float yaw{0.0F};
  LayoutRevisions revisions;
  bool valid{false};
  FormationLayout layout;
};

thread_local std::unordered_map<const Engine::Core::Entity*, SpatialLayoutCacheEntry>
    g_spatial_layout_cache;

constexpr std::size_t k_max_cached_spatial_layouts = 8192U;

void prune_spatial_layout_cache() {
  if (g_spatial_layout_cache.size() > k_max_cached_spatial_layouts) {
    g_spatial_layout_cache.clear();
  }
}

auto spatialized_layout_for(const Engine::Core::Entity& entity,
                            const FormationLayout& base_layout,
                            std::uint64_t base_signature) -> const FormationLayout& {
  auto const* transform = entity.get_component<Engine::Core::TransformComponent>();
  LayoutRevisions const revisions = layout_revisions_of(entity);

  float const world_x = transform != nullptr ? transform->position.x : 0.0F;
  float const world_z = transform != nullptr ? transform->position.z : 0.0F;
  float const yaw = transform != nullptr ? transform->rotation.y : 0.0F;

  SpatialLayoutCacheEntry& entry = g_spatial_layout_cache[&entity];
  if (entry.valid && entry.base_signature == base_signature &&
      entry.world_x == world_x && entry.world_z == world_z && entry.yaw == yaw &&
      entry.revisions == revisions) {
    return entry.layout;
  }

  spatialize_layout_into(entity, base_layout, entry.layout);
  entry.base_signature = base_signature;
  entry.world_x = world_x;
  entry.world_z = world_z;
  entry.yaw = yaw;
  entry.revisions = revisions;
  entry.valid = true;
  return entry.layout;
}

struct ResolvedContact {
  const FormationLayout* attacker_layout{nullptr};
  const FormationLayout* target_layout{nullptr};
  ContactGeometry geometry;
};

struct SlotsCacheEntry {
  std::uint64_t signature{0};
  bool has_slots{false};
  bool valid{false};
};

thread_local std::unordered_map<const Engine::Core::Entity*, SlotsCacheEntry>
    g_slots_cache;

auto has_formation_slots_for(const Engine::Core::Entity& entity,
                             std::uint64_t signature) -> bool {
  if (g_slots_cache.size() > k_max_cached_spatial_layouts) {
    g_slots_cache.clear();
  }
  SlotsCacheEntry& entry = g_slots_cache[&entity];
  if (entry.valid && entry.signature == signature) {
    return entry.has_slots;
  }
  entry.signature = signature;
  entry.has_slots = has_formation_slots(entity);
  entry.valid = true;
  return entry.has_slots;
}

struct ContactCacheKey {
  const Engine::Core::Entity* attacker{nullptr};
  const Engine::Core::Entity* target{nullptr};

  auto operator==(const ContactCacheKey&) const -> bool = default;
};

struct ContactCacheKeyHash {
  auto operator()(const ContactCacheKey& key) const noexcept -> std::size_t {
    std::uint64_t seed = 0xcbf29ce484222325ULL;
    hash_combine(
        seed,
        static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(key.attacker)));
    hash_combine(
        seed, static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(key.target)));
    return static_cast<std::size_t>(seed);
  }
};

struct ContactCacheStamp {
  std::uint64_t attacker_signature{0};
  std::uint64_t target_signature{0};
  float attacker_x{0.0F};
  float attacker_z{0.0F};
  float attacker_yaw{0.0F};
  float target_x{0.0F};
  float target_z{0.0F};
  float target_yaw{0.0F};
  float attacker_reach{0.0F};
  LayoutRevisions attacker_revisions;
  LayoutRevisions target_revisions;
  bool attacker_has_slots{false};
  bool target_has_slots{false};

  auto operator==(const ContactCacheStamp&) const -> bool = default;
};

struct ContactCacheEntry {
  ContactCacheStamp stamp;
  ContactGeometry geometry;
  bool valid{false};
};

thread_local std::unordered_map<ContactCacheKey, ContactCacheEntry, ContactCacheKeyHash>
    g_contact_cache;

constexpr std::size_t k_max_cached_contacts = 65536U;

struct SlotOffset {
  float world_x{0.0F};
  float world_z{0.0F};
  float offset_x{0.0F};
  float offset_z{0.0F};
};

struct AttackerSlotEntry {
  const SlotOffset* slot{nullptr};
  float parallel{0.0F};
};

struct ContactSlotsEntry {
  std::uint64_t signature{0};
  float world_x{0.0F};
  float world_z{0.0F};
  float yaw{0.0F};
  LayoutRevisions revisions;
  std::vector<SlotOffset> offsets;
  float min_x{0.0F};
  float max_x{0.0F};
  float min_z{0.0F};
  float max_z{0.0F};
  bool valid{false};
};

thread_local std::unordered_map<const Engine::Core::Entity*, ContactSlotsEntry>
    g_contact_slots_cache;

void prune_contact_slots_cache() {
  if (g_contact_slots_cache.size() > k_max_cached_spatial_layouts) {
    g_contact_slots_cache.clear();
  }
}

auto contact_slots_for(const Engine::Core::Entity& entity,
                       const FormationLayout& layout,
                       const Engine::Core::TransformComponent& transform,
                       std::uint64_t signature,
                       const LayoutRevisions& revisions) -> const ContactSlotsEntry& {
  ContactSlotsEntry& entry = g_contact_slots_cache[&entity];
  if (entry.valid && entry.signature == signature &&
      entry.world_x == transform.position.x && entry.world_z == transform.position.z &&
      entry.yaw == transform.rotation.y && entry.revisions == revisions) {
    return entry;
  }

  constexpr float k_infinity = std::numeric_limits<float>::infinity();
  entry.offsets.clear();
  entry.offsets.reserve(layout.occupied_slots.size());
  entry.min_x = k_infinity;
  entry.max_x = -k_infinity;
  entry.min_z = k_infinity;
  entry.max_z = -k_infinity;
  for (auto const& slot : layout.occupied_slots) {
    entry.offsets.push_back({.world_x = slot.world_x,
                             .world_z = slot.world_z,
                             .offset_x = slot.world_x - transform.position.x,
                             .offset_z = slot.world_z - transform.position.z});
    entry.min_x = std::min(entry.min_x, slot.world_x);
    entry.max_x = std::max(entry.max_x, slot.world_x);
    entry.min_z = std::min(entry.min_z, slot.world_z);
    entry.max_z = std::max(entry.max_z, slot.world_z);
  }
  entry.signature = signature;
  entry.world_x = transform.position.x;
  entry.world_z = transform.position.z;
  entry.yaw = transform.rotation.y;
  entry.revisions = revisions;
  entry.valid = true;
  return entry;
}

void accumulate_slot_contact(const ContactSlotsEntry& attacker_slots,
                             const ContactSlotsEntry& target_slots,
                             const FormationLayout& attacker_layout,
                             const FormationLayout& target_layout,
                             float dir_x,
                             float dir_z,
                             bool has_direction,
                             ContactGeometry& result) {
  const float contact_radius = attacker_layout.body_radius + target_layout.body_radius;
  const float contact_radius_sq = contact_radius * contact_radius;
  constexpr float k_infinity = std::numeric_limits<float>::infinity();

  auto const& target_offsets = target_slots.offsets;
  const float target_min_x = target_slots.min_x;
  const float target_max_x = target_slots.max_x;
  const float target_min_z = target_slots.min_z;
  const float target_max_z = target_slots.max_z;

  float nearest_target_parallel = k_infinity;
  for (auto const& target_slot : target_offsets) {
    nearest_target_parallel =
        std::min(nearest_target_parallel,
                 (target_slot.offset_x * dir_x) + (target_slot.offset_z * dir_z));
  }

  thread_local std::vector<AttackerSlotEntry> attacker_entries;
  attacker_entries.clear();
  attacker_entries.reserve(attacker_slots.offsets.size());
  for (auto const& attacker_slot : attacker_slots.offsets) {
    attacker_entries.push_back({.slot = &attacker_slot,
                                .parallel = (attacker_slot.offset_x * dir_x) +
                                            (attacker_slot.offset_z * dir_z)});
  }
  std::sort(attacker_entries.begin(),
            attacker_entries.end(),
            [](const AttackerSlotEntry& lhs, const AttackerSlotEntry& rhs) {
              return lhs.parallel > rhs.parallel;
            });

  float nearest_sq = k_infinity;
  for (auto const& attacker_entry : attacker_entries) {
    const SlotOffset& attacker_slot = *attacker_entry.slot;
    const float gap_x = std::max({0.0F,
                                  target_min_x - attacker_slot.world_x,
                                  attacker_slot.world_x - target_max_x});
    const float gap_z = std::max({0.0F,
                                  target_min_z - attacker_slot.world_z,
                                  attacker_slot.world_z - target_max_z});
    const bool may_shorten = ((gap_x * gap_x) + (gap_z * gap_z)) < nearest_sq;

    const float attacker_offset_x = attacker_slot.offset_x;
    const float attacker_offset_z = attacker_slot.offset_z;
    const float attacker_parallel = attacker_entry.parallel;
    const bool may_deepen =
        has_direction && (contact_radius - nearest_target_parallel +
                          attacker_parallel) > result.contact_center_distance;

    if (!may_shorten && !may_deepen) {
      continue;
    }

    for (auto const& target_slot : target_offsets) {
      if (may_shorten) {
        const float span_x = target_slot.world_x - attacker_slot.world_x;
        const float span_z = target_slot.world_z - attacker_slot.world_z;
        nearest_sq = std::min(nearest_sq, (span_x * span_x) + (span_z * span_z));
      }
      if (!may_deepen) {
        continue;
      }

      const float relative_x = target_slot.offset_x - attacker_offset_x;
      const float relative_z = target_slot.offset_z - attacker_offset_z;
      const float parallel = (relative_x * dir_x) + (relative_z * dir_z);
      if (contact_radius - parallel <= result.contact_center_distance) {
        continue;
      }
      const float relative_sq = (relative_x * relative_x) + (relative_z * relative_z);
      const float lateral_sq = std::max(0.0F, relative_sq - (parallel * parallel));
      if (lateral_sq > contact_radius_sq) {
        continue;
      }
      const float candidate = -parallel + std::sqrt(contact_radius_sq - lateral_sq);
      result.contact_center_distance =
          std::max(result.contact_center_distance, candidate);
    }
  }

  const float nearest = std::isinf(nearest_sq) ? nearest_sq : std::sqrt(nearest_sq);
  result.surface_gap =
      nearest - attacker_layout.body_radius - target_layout.body_radius;
}

void resolve_contact(const Engine::Core::Entity& attacker,
                     const Engine::Core::Entity& target,
                     ResolvedContact& resolved) {
  resolved.geometry = ContactGeometry{};
  resolved.attacker_layout = nullptr;
  resolved.target_layout = nullptr;
  ContactGeometry& result = resolved.geometry;
  auto const* attacker_transform =
      attacker.get_component<Engine::Core::TransformComponent>();
  auto const* target_transform =
      target.get_component<Engine::Core::TransformComponent>();
  if (attacker_transform == nullptr || target_transform == nullptr) {
    result.surface_gap = std::numeric_limits<float>::infinity();
    return;
  }

  float const dx = target_transform->position.x - attacker_transform->position.x;
  float const dz = target_transform->position.z - attacker_transform->position.z;
  result.center_distance = std::sqrt((dx * dx) + (dz * dz));

  std::uint64_t const attacker_signature = layout_signature(attacker);
  std::uint64_t const target_signature = layout_signature(target);
  bool const attacker_has_slots = has_formation_slots_for(attacker, attacker_signature);
  bool const target_has_slots = has_formation_slots_for(target, target_signature);
  result.uses_formation_slots = attacker_has_slots || target_has_slots;
  if (!result.uses_formation_slots) {

    result.surface_gap = result.center_distance;

    float const body_contact = single_body_radius(attacker, attacker_transform->scale) +
                               single_body_radius(target, target_transform->scale);
    float const reach = melee_reach(attacker);
    result.contact_center_distance = body_contact;
    result.engagement_center_distance =
        body_contact + std::max(0.0F, reach - body_contact) * k_single_body_reach_share;
    return;
  }

  const std::uint64_t generation_before = g_layout_cache_generation;
  const LayoutCacheEntry* attacker_entry =
      resolve_layout_entry(attacker, attacker_signature);
  const LayoutCacheEntry* target_entry = resolve_layout_entry(target, target_signature);
  if (g_layout_cache_generation != generation_before) {

    auto const refreshed = g_layout_cache.find(&attacker);
    attacker_entry = refreshed != g_layout_cache.end() ? &refreshed->second : nullptr;
  }

  static const FormationLayout k_absent_layout{};
  auto const& attacker_layout =
      attacker_entry != nullptr ? attacker_entry->layout : k_absent_layout;
  auto const& target_layout =
      target_entry != nullptr ? target_entry->layout : k_absent_layout;
  prune_spatial_layout_cache();
  resolved.attacker_layout = &spatialized_layout_for(
      attacker,
      attacker_layout,
      attacker_entry != nullptr ? attacker_entry->local_signature : 0U);
  resolved.target_layout = &spatialized_layout_for(
      target,
      target_layout,
      target_entry != nullptr ? target_entry->local_signature : 0U);
  auto const& spatial_attacker_layout = *resolved.attacker_layout;
  auto const& spatial_target_layout = *resolved.target_layout;

  LayoutRevisions const attacker_revisions = layout_revisions_of(attacker);
  LayoutRevisions const target_revisions = layout_revisions_of(target);
  ContactCacheStamp const stamp{.attacker_signature = attacker_signature,
                                .target_signature = target_signature,
                                .attacker_x = attacker_transform->position.x,
                                .attacker_z = attacker_transform->position.z,
                                .attacker_yaw = attacker_transform->rotation.y,
                                .target_x = target_transform->position.x,
                                .target_z = target_transform->position.z,
                                .target_yaw = target_transform->rotation.y,
                                .attacker_reach = melee_reach(attacker),
                                .attacker_revisions = attacker_revisions,
                                .target_revisions = target_revisions,
                                .attacker_has_slots = attacker_has_slots,
                                .target_has_slots = target_has_slots};

  if (g_contact_cache.size() > k_max_cached_contacts) {
    g_contact_cache.clear();
  }
  ContactCacheEntry& contact_cache =
      g_contact_cache[ContactCacheKey{.attacker = &attacker, .target = &target}];
  if (contact_cache.valid && contact_cache.stamp == stamp) {
    result = contact_cache.geometry;
    return;
  }

  result.formation_overlap_required = attacker_has_slots && target_has_slots;
  result.contact_tolerance =
      std::min(spatial_attacker_layout.body_radius, spatial_target_layout.body_radius) *
      0.15F;

  const bool has_direction = result.center_distance > 0.0001F;
  const float dir_x = has_direction ? dx / result.center_distance : 0.0F;
  const float dir_z = has_direction ? dz / result.center_distance : 0.0F;
  prune_contact_slots_cache();
  accumulate_slot_contact(contact_slots_for(attacker,
                                            spatial_attacker_layout,
                                            *attacker_transform,
                                            attacker_signature,
                                            attacker_revisions),
                          contact_slots_for(target,
                                            spatial_target_layout,
                                            *target_transform,
                                            target_signature,
                                            target_revisions),
                          spatial_attacker_layout,
                          spatial_target_layout,
                          dir_x,
                          dir_z,
                          has_direction,
                          result);

  if (result.formation_overlap_required) {

    float const rank_spacing =
        std::min(spatial_attacker_layout.spacing, spatial_target_layout.spacing);
    float const body_radius = std::max(spatial_attacker_layout.body_radius,
                                       spatial_target_layout.body_radius);
    result.engagement_center_distance =
        std::min(result.contact_center_distance,
                 std::max(rank_spacing * 0.30F, body_radius * 0.5F));
    result.body_contact_center_distance =
        std::max(spatial_attacker_layout.body_radius, k_body_core_radius_floor) +
        std::max(spatial_target_layout.body_radius, k_body_core_radius_floor);
  } else if (attacker.has_component<Engine::Core::ElephantComponent>() &&
             has_formation_slots(target)) {

    result.engagement_center_distance = std::max(
        0.0F,
        result.center_distance - (result.surface_gap + k_elephant_chase_penetration));
  } else {

    result.engagement_center_distance =
        std::max(0.0F,
                 result.center_distance - result.surface_gap +
                     stamp.attacker_reach * k_mixed_body_approach_margin);
  }

  contact_cache.stamp = stamp;
  contact_cache.geometry = result;
  contact_cache.valid = true;
}

} // namespace

auto resolve_contact_context(const Engine::Core::Entity& attacker,
                             const Engine::Core::Entity& target)
    -> FormationContactContext {
  ResolvedContact resolved;
  resolve_contact(attacker, target, resolved);
  FormationContactContext context;
  context.geometry = resolved.geometry;
  if (resolved.attacker_layout != nullptr) {
    context.attacker_layout = *resolved.attacker_layout;
  }
  if (resolved.target_layout != nullptr) {
    context.target_layout = *resolved.target_layout;
  }
  return context;
}

auto contact_geometry(const Engine::Core::Entity& attacker,
                      const Engine::Core::Entity& target) -> ContactGeometry {
  thread_local ResolvedContact scratch;
  resolve_contact(attacker, target, scratch);
  return scratch.geometry;
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

    bool const bodies_are_in_contact =
        geometry.body_contact_center_distance > k_contact_numeric_epsilon &&
        geometry.center_distance <=
            geometry.body_contact_center_distance + k_contact_numeric_epsilon;

    return deep_front_rank_overlap || locked_visible_overlap ||
           degenerate_slot_contact || bodies_are_in_contact;
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
  return engagement_pairs(
      attacker, target, resolve_layout(attacker), resolve_layout(target));
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

  FormationLayout spatial_attacker_layout;
  FormationLayout spatial_target_layout;
  spatialize_layout_into(attacker, attacker_layout, spatial_attacker_layout);
  spatialize_layout_into(target, target_layout, spatial_target_layout);
  float const contact_distance =
      spatial_attacker_layout.body_radius + spatial_target_layout.body_radius;
  for (auto const& attacker_slot : spatial_attacker_layout.live_slots) {
    auto const* closest_target = &spatial_target_layout.live_slots.front();
    float closest_distance = slot_distance(attacker_slot, *closest_target);
    for (auto const& target_slot : spatial_target_layout.live_slots) {
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
  std::size_t contact_candidate_count = 0U;
  for (auto const& pair : pairs) {
    if (pair.surface_gap <= closest->surface_gap + equivalent_contact_band) {
      ++contact_candidate_count;
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
  std::size_t selected_index = seed % contact_candidate_count;
  for (auto const& pair : pairs) {
    if (pair.surface_gap > closest->surface_gap + equivalent_contact_band) {
      continue;
    }
    if (selected_index == 0U) {
      return pair;
    }
    --selected_index;
  }
  return std::nullopt;
}

void invalidate_layout_cache() {
  g_layout_cache.clear();
  ++g_layout_cache_generation;
  g_spatial_layout_cache.clear();
  g_slots_cache.clear();
  g_contact_cache.clear();
  g_contact_slots_cache.clear();
}

} // namespace Game::Systems::FormationCombat
