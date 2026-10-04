#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include "../core/component.h"
#include "../formation/traversal_layout_policy.h"
#include "../formation/unit_layout_resolver.h"
#include "../formation/unit_layout_state.h"
#include "formation_combat_geometry.h"
#include "formation_geometry_internal.h"

namespace Game::Systems::FormationCombat {
namespace Detail {
namespace {

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

void transform_cached_slots(FormationLayout& layout,
                            const Engine::Core::TransformComponent& transform) {
  float const yaw = transform.rotation.y * std::numbers::pi_v<float> / 180.0F;
  float const sin_yaw = std::sin(yaw);
  float const cos_yaw = std::cos(yaw);
  float const origin_x = transform.position.x;
  float const origin_z = transform.position.z;
  auto update = [&](std::vector<SoldierSlot>& soldier_slots) {
    for (SoldierSlot& slot : soldier_slots) {
      slot.world_x = origin_x + cos_yaw * slot.local_x + sin_yaw * slot.local_z;
      slot.world_z = origin_z - sin_yaw * slot.local_x + cos_yaw * slot.local_z;
    }
  };
  update(layout.all_slots);
  update(layout.live_slots);
  update(layout.occupied_slots);
}

void store_layout_cache(const Engine::Core::Entity& entity,
                        std::uint64_t signature,
                        const Engine::Core::TransformComponent& transform,
                        const FormationLayout& layout) {
  auto& state = caches();
  const std::size_t before = state.layout.size();
  evict_incrementally(state.layout, k_max_cached_layouts);
  if (state.layout.size() != before) {
    ++state.layout_generation;
  }
  LayoutCacheEntry& cache = state.layout[cache_key(entity)];
  cache.epoch = formation_cache_epoch();
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
  cache.extents = compute_formation_extents(layout);
}

auto is_rigid_body(const Engine::Core::Entity& entity) -> bool {
  return entity.has_component<Engine::Core::BuildingComponent>() ||
         entity.has_component<Engine::Core::ElephantComponent>();
}

auto body_radius_from_scale(const Engine::Core::TransformComponent& transform)
    -> float {
  return std::max(0.05F, std::max(transform.scale.x, transform.scale.z) * 0.5F);
}

auto build_rigid_body_layout(const Engine::Core::Entity& entity,
                             const Engine::Core::UnitComponent& unit,
                             const Engine::Core::TransformComponent& transform)
    -> FormationLayout {
  FormationLayout result;
  result.total_count = 1;
  result.live_count = unit.health > 0 ? 1 : 0;
  result.rows = 1;
  result.cols = 1;
  result.body_radius = body_radius_from_scale(transform);
  if (entity.has_component<Engine::Core::ElephantComponent>()) {
    float const visual_scale = std::max(
        0.05F, std::max(std::abs(transform.scale.x), std::abs(transform.scale.z)));
    result.body_radius =
        std::max(result.body_radius, k_elephant_visual_body_radius * visual_scale);
  }
  result.seed = formation_seed(entity);
  if (result.live_count > 0) {
    SoldierSlot const slot{
        0U, 0U, 0U, 0.0F, 0.0F, 0.0F, transform.position.x, transform.position.z};
    result.all_slots.push_back(slot);
    result.live_slots.push_back(slot);
    result.occupied_slots.push_back(slot);
  }
  return result;
}

struct SlotPlacer {
  const FormationDefinition& definition;
  const FormationLayout& layout;
  const WorkSite& work_site;
  const Engine::Core::TransformComponent& transform;

  [[nodiscard]] auto place(int stable_idx,
                           int layout_idx,
                           int layout_count,
                           int layout_rows,
                           int layout_cols) const -> SoldierSlot {
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
    query.spacing = layout.spacing;
    query.seed = layout.seed;
    auto offset = Game::Formation::UnitLayoutSystem::instance().offset(query);
    if (work_site.active) {
      place_on_site_perimeter(work_site, offset.offset_x, offset.offset_z);
    }
    auto const [world_x, world_z] =
        world_slot(transform, offset.offset_x, offset.offset_z);
    return SoldierSlot{static_cast<std::uint16_t>(stable_idx),
                       static_cast<std::uint16_t>(row),
                       static_cast<std::uint16_t>(col),
                       offset.offset_x,
                       offset.offset_z,
                       offset.yaw_offset,
                       world_x,
                       world_z};
  }
};

auto living_slot_mask(const Engine::Core::Entity& entity,
                      int total_count) -> std::vector<bool> {
  std::vector<bool> live(static_cast<std::size_t>(total_count), false);
  for (auto const slot : living_slot_indices(entity, total_count)) {
    live[static_cast<std::size_t>(slot)] = true;
  }
  return live;
}

auto casualty_slot_mask(
    const Engine::Core::SoldierCasualtyAnimationComponent* casualties,
    int total_count) -> std::vector<bool> {
  std::vector<bool> active(static_cast<std::size_t>(total_count), false);
  if (casualties != nullptr) {
    for (auto const& casualty : casualties->entries) {
      int const idx = static_cast<int>(casualty.slot_index);
      if (idx >= 0 && idx < total_count) {
        active[static_cast<std::size_t>(idx)] = true;
      }
    }
  }
  return active;
}

void place_live_slots(FormationLayout& result,
                      const SlotPlacer& placer,
                      const std::vector<bool>& live_slots,
                      bool preserve_stable_slots) {
  int const compact_cols = std::max(1, std::min(result.cols, result.live_count));
  int const compact_rows =
      std::max(1, (result.live_count + compact_cols - 1) / compact_cols);
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
        placer.place(stable_idx, layout_idx, layout_count, layout_rows, layout_cols);
    result.live_slots.push_back(slot);
    result.occupied_slots.push_back(slot);
    ++compact_idx;
  }
}

void append_casualty_slots(
    FormationLayout& result,
    const Engine::Core::TransformComponent& transform,
    const Engine::Core::SoldierCasualtyAnimationComponent* casualties,
    const std::vector<bool>& live_slots,
    const std::vector<bool>& active_casualty_slots) {
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
            world_slot(transform, found->local_x, found->local_z);
        casualty_slot.world_x = world_x;
        casualty_slot.world_z = world_z;
      }
    }
    result.occupied_slots.push_back(casualty_slot);
  }
}

// The layout state of a troop that has closed into its defensive layout (a
// testudo, a shield wall), else nullptr.
auto defensive_layout_state(const Engine::Core::Entity& entity)
    -> const Engine::Core::UnitLayoutStateComponent* {
  const auto* registry = entity.registry();
  const auto* layout =
      registry != nullptr
          ? registry->try_get<Engine::Core::UnitLayoutStateComponent>(entity.get_id())
          : nullptr;
  return layout != nullptr &&
                 layout->state == static_cast<std::uint8_t>(
                                      Game::Formation::UnitLayoutState::Defensive)
             ? layout
             : nullptr;
}

auto build_formation_layout(const Engine::Core::Entity& entity,
                            const Engine::Core::UnitComponent& unit,
                            const Engine::Core::TransformComponent& transform)
    -> FormationLayout {
  FormationLayout result;
  auto definition = resolve_definition(unit);
  // A troop that has closed into a testudo or a shield wall stands in that
  // layout's slots, not its marching ones: the men close the gaps.
  if (auto const* layout_state = defensive_layout_state(entity);
      layout_state != nullptr &&
      layout_state->layout_id != Game::Formation::k_invalid_layout) {
    definition.layout = layout_state->layout_id;
  }
  auto const work_site = work_site_for(entity, transform);
  if (work_site.active) {
    definition.layout = Game::Formation::UnitLayoutLibrary::instance().resolve(
        definition.doctrine, "work_party");
  }
  result.total_count = definition.total_count;
  result.cols = definition.max_per_row;
  result.rows = std::max(1, (result.total_count + result.cols - 1) / result.cols);
  result.spacing = definition.spacing;
  result.body_radius = body_radius_from_scale(transform);
  result.seed = formation_seed(entity);

  result.all_slots.reserve(static_cast<std::size_t>(result.total_count));
  result.live_slots.reserve(static_cast<std::size_t>(result.total_count));
  result.occupied_slots.reserve(static_cast<std::size_t>(result.total_count));
  SlotPlacer const placer{definition, result, work_site, transform};
  for (int idx = 0; idx < result.total_count; ++idx) {
    result.all_slots.push_back(
        placer.place(idx, idx, result.total_count, result.rows, result.cols));
  }

  std::vector<bool> const live_slots = living_slot_mask(entity, result.total_count);
  result.live_count =
      static_cast<int>(std::count(live_slots.begin(), live_slots.end(), true));
  auto const* casualties =
      entity.get_component<Engine::Core::SoldierCasualtyAnimationComponent>();
  std::vector<bool> const active_casualty_slots =
      casualty_slot_mask(casualties, result.total_count);

  bool const remnant_closes_ranks = result.live_count <= result.cols;
  bool const preserve_stable_slots =
      holds_formation_line(entity) || !remnant_closes_ranks;
  place_live_slots(result, placer, live_slots, preserve_stable_slots);
  append_casualty_slots(
      result, transform, casualties, live_slots, active_casualty_slots);
  return result;
}

void build_layout_into_cache(const Engine::Core::Entity& entity,
                             const Engine::Core::UnitComponent& unit,
                             const Engine::Core::TransformComponent& transform,
                             std::uint64_t signature) {
  FormationLayout result = is_rigid_body(entity)
                               ? build_rigid_body_layout(entity, unit, transform)
                               : build_formation_layout(entity, unit, transform);
  store_layout_cache(entity, signature, transform, result);
}

} // namespace

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

auto compute_formation_extents(const FormationLayout& layout) -> FormationExtents {
  FormationExtents extents;
  extents.body_radius = layout.body_radius;
  for (auto const& slot : layout.live_slots) {
    extents.live_envelope =
        std::max(extents.live_envelope,
                 std::hypot(slot.local_x, slot.local_z) + layout.body_radius);
  }
  extents.minimum_scale = minimum_formation_scale(layout);
  float lateral_extent = layout.body_radius;
  for (auto const& slot : layout.live_slots) {
    lateral_extent =
        std::max(lateral_extent,
                 std::abs(slot.local_x) * extents.minimum_scale + layout.body_radius);
  }
  extents.navigation_clearance = std::max(0.1F, lateral_extent);
  return extents;
}

auto layout_signature(const Engine::Core::Entity& entity) -> std::uint64_t {
  std::uint64_t signature = 0xcbf29ce484222325ULL;
  hash_combine(signature, entity.get_id());
  bool const is_building = entity.has_component<Engine::Core::BuildingComponent>();
  bool const is_elephant = entity.has_component<Engine::Core::ElephantComponent>();
  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  if (unit != nullptr) {
    hash_combine(signature, static_cast<std::uint64_t>(unit->spawn_type));
    hash_combine(signature, static_cast<std::uint64_t>(unit->owner_id));
    hash_combine(signature, static_cast<std::uint64_t>(unit->nation_id));
    hash_combine(
        signature,
        static_cast<std::uint64_t>(unit->render_individuals_per_unit_override));
    hash_combine(signature, static_cast<std::uint64_t>(unit->formation_files_override));
    hash_combine(signature, static_cast<std::uint64_t>(unit->squad_strength));
    hash_combine(signature, unit->uses_nation_formation_profile ? 1U : 0U);
    if (auto const* layout_state = defensive_layout_state(entity);
        layout_state != nullptr) {
      hash_combine(signature, static_cast<std::uint64_t>(layout_state->layout_id));
    }

    bool const rigid_body = is_building || is_elephant;
    int const health_state =
        rigid_body ? (unit->health > 0 ? 1 : 0)
                   : Engine::Core::resolve_surviving_individual_count(
                         unit->health, unit->max_health, definition_total_count(*unit));
    hash_combine(signature, static_cast<std::uint64_t>(health_state));
  }
  auto const* transform = entity.get_component<Engine::Core::TransformComponent>();
  if (transform != nullptr) {
    hash_float(signature, transform->scale.x);
    hash_float(signature, transform->scale.z);
  }
  if (auto const* builder =
          entity.get_component<Engine::Core::BuilderProductionComponent>()) {
    bool const working = builder->in_progress && builder->at_construction_site;
    hash_combine(signature, working ? 1U : 0U);
    if (working) {
      hash_combine(signature, std::hash<std::string>{}(builder->product_type));
      hash_float(signature, builder->construction_site_x);
      hash_float(signature, builder->construction_site_z);
      hash_float(signature, builder->construction_site_rotation_y);
    }
  }
  hash_combine(signature, is_building ? 1U : 0U);
  hash_combine(signature, is_elephant ? 1U : 0U);
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
  auto& layout_cache = caches().layout;
  if (auto cached = layout_cache.find(cache_key(entity));
      cached != layout_cache.end() && cached->second.epoch == formation_cache_epoch() &&
      cached->second.local_signature == signature) {
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
  auto const rebuilt = layout_cache.find(cache_key(entity));
  return rebuilt != layout_cache.end() ? &rebuilt->second : nullptr;
}

auto resolve_layout_entry(const Engine::Core::Entity& entity)
    -> const LayoutCacheEntry* {
  return resolve_layout_entry(entity, layout_signature(entity));
}

} // namespace Detail

auto resolve_layout(const Engine::Core::Entity& entity) -> FormationLayout {
  auto const* entry = Detail::resolve_layout_entry(entity);
  return entry != nullptr ? entry->layout : FormationLayout{};
}

void resolve_layout_into(const Engine::Core::Entity& entity, FormationLayout& out) {
  if (auto const* entry = Detail::resolve_layout_entry(entity)) {
    out = entry->layout;
    return;
  }
  out = FormationLayout{};
}

auto formation_definition_epoch() -> std::uint64_t {
  return Detail::formation_cache_epoch();
}

auto resolve_layout_spacing(const Engine::Core::Entity& entity) -> float {
  auto const* entry = Detail::resolve_layout_entry(entity);
  return entry != nullptr ? entry->layout.spacing : FormationLayout{}.spacing;
}

auto formation_turn_radius(const Engine::Core::Entity& entity) -> float {
  std::uint64_t const signature = Detail::layout_signature(entity);
  auto& layout_cache = Detail::caches().layout;
  auto cached = layout_cache.find(Detail::cache_key(entity));
  if (cached == layout_cache.end() ||
      cached->second.epoch != Detail::formation_cache_epoch() ||
      cached->second.local_signature != signature) {
    (void)resolve_layout(entity);
    cached = layout_cache.find(Detail::cache_key(entity));
  }
  return cached != layout_cache.end() ? cached->second.turn_radius : 0.0F;
}

auto formation_lateral_half_extent(const FormationLayout& layout) -> float {
  float extent = layout.body_radius;
  auto const& measured =
      layout.live_slots.empty() ? layout.all_slots : layout.live_slots;
  for (auto const& slot : measured) {
    extent = std::max(extent, std::abs(slot.local_x) + layout.body_radius);
  }
  return extent;
}

auto minimum_formation_scale(const FormationLayout& layout) -> float {
  float nearest = std::numeric_limits<float>::infinity();
  for (std::size_t first = 0; first < layout.all_slots.size(); ++first) {
    for (std::size_t second = first + 1; second < layout.all_slots.size(); ++second) {
      auto const& a = layout.all_slots[first];
      auto const& b = layout.all_slots[second];
      float const distance = std::hypot(a.local_x - b.local_x, a.local_z - b.local_z);
      if (distance > 0.001F) {
        nearest = std::min(nearest, distance);
      }
    }
  }
  return std::isfinite(nearest)
             ? std::min(1.0F,
                        Game::Formation::TraversalPolicy::compact_spacing(
                            layout.body_radius, nearest) /
                            nearest)
             : 1.0F;
}

auto formation_extents(const Engine::Core::Entity& entity) -> FormationExtents {
  if (auto const* entry = Detail::resolve_layout_entry(entity)) {
    return entry->extents;
  }
  return Detail::compute_formation_extents(FormationLayout{});
}

auto formation_navigation_clearance(const Engine::Core::Entity& entity) -> float {
  return formation_extents(entity).navigation_clearance;
}

} // namespace Game::Systems::FormationCombat
