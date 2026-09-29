#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "../core/component.h"
#include "formation_combat_geometry.h"
#include "formation_geometry_internal.h"

namespace Game::Systems::FormationCombat {
namespace {

using namespace Detail;

constexpr float k_elephant_chase_penetration = 1.50F;
constexpr float k_elephant_contact_penetration = 1.10F;

constexpr float k_single_body_reach_share = 0.20F;

constexpr float k_mixed_body_approach_margin = 0.5F;

constexpr float k_single_body_strike_share = 0.55F;

constexpr float k_engagement_close_slack = 0.25F;

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

struct ResolvedContact {
  const FormationLayout* attacker_layout{nullptr};
  const FormationLayout* target_layout{nullptr};
  ContactGeometry geometry;
};

auto has_formation_slots_for(const Engine::Core::Entity& entity,
                             std::uint64_t signature) -> bool {
  if (caches().slot_presence.size() > k_max_cached_layouts) {
    caches().slot_presence.clear();
  }
  SlotsCacheEntry& entry = caches().slot_presence[cache_key(entity)];
  if (entry.epoch != formation_cache_epoch()) {
    entry = SlotsCacheEntry{};
    entry.epoch = formation_cache_epoch();
  }
  if (entry.valid && entry.signature == signature) {
    return entry.has_slots;
  }
  entry.signature = signature;
  entry.has_slots = has_formation_slots(entity);
  entry.valid = true;
  return entry.has_slots;
}

struct AttackerSlotEntry {
  const SlotOffset* slot{nullptr};
  float parallel{0.0F};
};

struct TargetSlotEntry {
  const SlotOffset* slot{nullptr};
  float parallel{0.0F};
};

auto contact_slots_for(const Engine::Core::Entity& entity,
                       const FormationLayout& layout,
                       const Engine::Core::TransformComponent& transform,
                       std::uint64_t signature,
                       const LayoutRevisions& revisions) -> const ContactSlotsEntry& {
  ContactSlotsEntry& entry = caches().contact_slots[cache_key(entity)];
  if (entry.epoch != formation_cache_epoch()) {
    entry = ContactSlotsEntry{};
    entry.epoch = formation_cache_epoch();
  }
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

struct ContactFrame {
  float dir_x{0.0F};
  float dir_z{0.0F};
  bool has_direction{false};
  float contact_radius{0.0F};
  float contact_radius_sq{0.0F};
  float nearest_target_parallel{std::numeric_limits<float>::infinity()};
  float target_min_x{0.0F};
  float target_max_x{0.0F};
  float target_min_z{0.0F};
  float target_max_z{0.0F};
};

void sort_target_entries(const ContactSlotsEntry& target_slots,
                         ContactFrame& frame,
                         std::vector<TargetSlotEntry>& target_entries) {
  target_entries.clear();
  target_entries.reserve(target_slots.offsets.size());
  for (auto const& target_slot : target_slots.offsets) {
    const float parallel =
        (target_slot.offset_x * frame.dir_x) + (target_slot.offset_z * frame.dir_z);
    frame.nearest_target_parallel = std::min(frame.nearest_target_parallel, parallel);
    target_entries.push_back({.slot = &target_slot, .parallel = parallel});
  }
  std::sort(target_entries.begin(),
            target_entries.end(),
            [](const TargetSlotEntry& lhs, const TargetSlotEntry& rhs) {
              return lhs.parallel < rhs.parallel;
            });
}

void sort_attacker_entries(const ContactSlotsEntry& attacker_slots,
                           const ContactFrame& frame,
                           std::vector<AttackerSlotEntry>& attacker_entries) {
  attacker_entries.clear();
  attacker_entries.reserve(attacker_slots.offsets.size());
  for (auto const& attacker_slot : attacker_slots.offsets) {
    attacker_entries.push_back({.slot = &attacker_slot,
                                .parallel = (attacker_slot.offset_x * frame.dir_x) +
                                            (attacker_slot.offset_z * frame.dir_z)});
  }
  std::sort(attacker_entries.begin(),
            attacker_entries.end(),
            [](const AttackerSlotEntry& lhs, const AttackerSlotEntry& rhs) {
              return lhs.parallel > rhs.parallel;
            });
}

void scan_attacker_slot(const AttackerSlotEntry& attacker_entry,
                        const std::vector<TargetSlotEntry>& target_entries,
                        const ContactFrame& frame,
                        float& nearest_sq,
                        ContactGeometry& result) {
  const SlotOffset& attacker_slot = *attacker_entry.slot;
  const float gap_x = std::max({0.0F,
                                frame.target_min_x - attacker_slot.world_x,
                                attacker_slot.world_x - frame.target_max_x});
  const float gap_z = std::max({0.0F,
                                frame.target_min_z - attacker_slot.world_z,
                                attacker_slot.world_z - frame.target_max_z});
  const bool may_shorten = ((gap_x * gap_x) + (gap_z * gap_z)) < nearest_sq;

  const float attacker_offset_x = attacker_slot.offset_x;
  const float attacker_offset_z = attacker_slot.offset_z;
  const float attacker_parallel = attacker_entry.parallel;
  const bool may_deepen =
      frame.has_direction && (frame.contact_radius - frame.nearest_target_parallel +
                              attacker_parallel) > result.contact_center_distance;

  if (!may_shorten && !may_deepen) {
    return;
  }

  bool deepen_exhausted = !may_deepen;
  for (auto const& target_entry : target_entries) {
    if (!may_shorten && deepen_exhausted) {
      break;
    }
    const SlotOffset& target_slot = *target_entry.slot;
    ++caches().stats.slot_pairs_examined;
    if (may_shorten) {
      const float span_x = target_slot.world_x - attacker_slot.world_x;
      if ((span_x * span_x) < nearest_sq) {
        const float span_z = target_slot.world_z - attacker_slot.world_z;
        nearest_sq = std::min(nearest_sq, (span_x * span_x) + (span_z * span_z));
      }
    }
    if (deepen_exhausted) {
      continue;
    }

    const float parallel = target_entry.parallel - attacker_parallel;
    if (frame.contact_radius - parallel <= result.contact_center_distance) {
      deepen_exhausted = true;
      continue;
    }

    const float relative_x = target_slot.offset_x - attacker_offset_x;
    const float relative_z = target_slot.offset_z - attacker_offset_z;
    const float relative_sq = (relative_x * relative_x) + (relative_z * relative_z);
    const float lateral_sq = std::max(0.0F, relative_sq - (parallel * parallel));
    if (lateral_sq > frame.contact_radius_sq) {
      continue;
    }
    const float candidate = -parallel + std::sqrt(frame.contact_radius_sq - lateral_sq);
    result.contact_center_distance =
        std::max(result.contact_center_distance, candidate);
  }
}

void accumulate_slot_contact(const ContactSlotsEntry& attacker_slots,
                             const ContactSlotsEntry& target_slots,
                             const FormationLayout& attacker_layout,
                             const FormationLayout& target_layout,
                             float dir_x,
                             float dir_z,
                             bool has_direction,
                             ContactGeometry& result) {
  ContactFrame frame;
  frame.dir_x = dir_x;
  frame.dir_z = dir_z;
  frame.has_direction = has_direction;
  frame.contact_radius = attacker_layout.body_radius + target_layout.body_radius;
  frame.contact_radius_sq = frame.contact_radius * frame.contact_radius;
  frame.target_min_x = target_slots.min_x;
  frame.target_max_x = target_slots.max_x;
  frame.target_min_z = target_slots.min_z;
  frame.target_max_z = target_slots.max_z;

  thread_local std::vector<TargetSlotEntry> target_entries;
  sort_target_entries(target_slots, frame, target_entries);
  caches().stats.slot_pairs_possible +=
      static_cast<std::uint64_t>(attacker_slots.offsets.size()) *
      static_cast<std::uint64_t>(target_slots.offsets.size());

  thread_local std::vector<AttackerSlotEntry> attacker_entries;
  sort_attacker_entries(attacker_slots, frame, attacker_entries);

  float nearest_sq = std::numeric_limits<float>::infinity();
  for (auto const& attacker_entry : attacker_entries) {
    scan_attacker_slot(attacker_entry, target_entries, frame, nearest_sq, result);
  }

  const float nearest = std::isinf(nearest_sq) ? nearest_sq : std::sqrt(nearest_sq);
  result.surface_gap =
      nearest - attacker_layout.body_radius - target_layout.body_radius;
}

void resolve_single_body_contact(
    const Engine::Core::Entity& attacker,
    const Engine::Core::Entity& target,
    const Engine::Core::TransformComponent& attacker_transform,
    const Engine::Core::TransformComponent& target_transform,
    ContactGeometry& result) {
  result.surface_gap = result.center_distance;

  float const body_contact = single_body_radius(attacker, attacker_transform.scale) +
                             single_body_radius(target, target_transform.scale);
  float const reach = melee_reach(attacker);
  result.contact_center_distance = body_contact;
  result.engagement_center_distance =
      body_contact + std::max(0.0F, reach - body_contact) * k_single_body_reach_share;
}

void resolve_spatial_layouts(const Engine::Core::Entity& attacker,
                             const Engine::Core::Entity& target,
                             std::uint64_t attacker_signature,
                             std::uint64_t target_signature,
                             ResolvedContact& resolved) {
  auto& state = caches();
  const std::uint64_t generation_before = state.layout_generation;
  const LayoutCacheEntry* attacker_entry =
      resolve_layout_entry(attacker, attacker_signature);
  const LayoutCacheEntry* target_entry = resolve_layout_entry(target, target_signature);
  if (state.layout_generation != generation_before) {
    auto const refreshed = state.layout.find(cache_key(attacker));
    attacker_entry = refreshed != state.layout.end() ? &refreshed->second : nullptr;
  }

  static const FormationLayout k_absent_layout{};
  auto const& attacker_layout =
      attacker_entry != nullptr ? attacker_entry->layout : k_absent_layout;
  auto const& target_layout =
      target_entry != nullptr ? target_entry->layout : k_absent_layout;
  evict_incrementally(state.spatial, k_max_cached_layouts);
  resolved.attacker_layout = &spatialized_layout_for(
      attacker,
      attacker_layout,
      attacker_entry != nullptr ? attacker_entry->local_signature : 0U);
  resolved.target_layout = &spatialized_layout_for(
      target,
      target_layout,
      target_entry != nullptr ? target_entry->local_signature : 0U);
}

void resolve_engagement_distance(const Engine::Core::Entity& attacker,
                                 const Engine::Core::Entity& target,
                                 const FormationLayout& attacker_layout,
                                 const FormationLayout& target_layout,
                                 float attacker_reach,
                                 ContactGeometry& result) {
  if (result.formation_overlap_required) {
    float const rank_spacing = std::min(attacker_layout.spacing, target_layout.spacing);
    float const body_radius =
        std::max(attacker_layout.body_radius, target_layout.body_radius);
    result.engagement_center_distance =
        std::min(result.contact_center_distance,
                 std::max(rank_spacing * 0.30F, body_radius * 0.5F));
    result.body_contact_center_distance =
        std::max(attacker_layout.body_radius, k_body_core_radius_floor) +
        std::max(target_layout.body_radius, k_body_core_radius_floor);
  } else if (attacker.has_component<Engine::Core::ElephantComponent>() &&
             has_formation_slots(target)) {
    result.engagement_center_distance = std::max(
        0.0F,
        result.center_distance - (result.surface_gap + k_elephant_chase_penetration));
  } else {
    result.engagement_center_distance =
        std::max(0.0F,
                 result.center_distance - result.surface_gap +
                     attacker_reach * k_mixed_body_approach_margin);
  }
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
    resolve_single_body_contact(
        attacker, target, *attacker_transform, *target_transform, result);
    return;
  }

  resolve_spatial_layouts(
      attacker, target, attacker_signature, target_signature, resolved);
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

  auto& state = caches();
  evict_incrementally(state.contact, k_max_cached_contacts);
  ContactCacheEntry& contact_cache = state.contact[ContactCacheKey{
      .attacker = cache_key(attacker), .target = cache_key(target)}];
  ++state.stats.resolutions;
  if (contact_cache.epoch != formation_cache_epoch()) {
    contact_cache = ContactCacheEntry{};
    contact_cache.epoch = formation_cache_epoch();
  }
  if (contact_cache.valid && contact_cache.stamp == stamp) {
    ++state.stats.cache_hits;
    result = contact_cache.geometry;
    return;
  }
  ++state.stats.cache_misses;

  result.formation_overlap_required = attacker_has_slots && target_has_slots;
  result.contact_tolerance =
      std::min(spatial_attacker_layout.body_radius, spatial_target_layout.body_radius) *
      0.15F;

  const bool has_direction = result.center_distance > 0.0001F;
  const float dir_x = has_direction ? dx / result.center_distance : 0.0F;
  const float dir_z = has_direction ? dz / result.center_distance : 0.0F;
  evict_incrementally(state.contact_slots, k_max_cached_layouts);
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
  resolve_engagement_distance(attacker,
                              target,
                              spatial_attacker_layout,
                              spatial_target_layout,
                              stamp.attacker_reach,
                              result);

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

    auto const* registry = target.registry();
    auto const* target_movement =
        registry->try_get<Engine::Core::MovementComponent>(target.get_id());
    auto const* target_attack =
        registry->try_get<Engine::Core::AttackComponent>(target.get_id());
    auto const* attacker_transform =
        registry->try_get<Engine::Core::TransformComponent>(attacker.get_id());
    auto const* target_transform =
        registry->try_get<Engine::Core::TransformComponent>(target.get_id());
    bool target_walks_past_or_away = false;
    if (target_movement != nullptr && target_movement->get_has_target() &&
        (target_attack == nullptr || !target_attack->in_melee_lock) &&
        attacker_transform != nullptr && target_transform != nullptr) {
      constexpr float k_walking_speed = 0.2F;
      constexpr float k_closing_cosine = 0.5F;
      float const vx = target_movement->get_vx();
      float const vz = target_movement->get_vz();
      float const speed = std::hypot(vx, vz);
      float const to_attacker_x =
          attacker_transform->position.x - target_transform->position.x;
      float const to_attacker_z =
          attacker_transform->position.z - target_transform->position.z;
      float const distance = std::hypot(to_attacker_x, to_attacker_z);
      target_walks_past_or_away = speed > k_walking_speed &&
                                  distance > k_contact_numeric_epsilon &&
                                  (vx * to_attacker_x + vz * to_attacker_z) <
                                      k_closing_cosine * speed * distance;
    }
    bool const soldiers_touch_a_moving_target =
        target_walks_past_or_away && geometry.surface_gap <= k_contact_numeric_epsilon;

    return deep_front_rank_overlap || locked_visible_overlap ||
           degenerate_slot_contact || bodies_are_in_contact ||
           soldiers_touch_a_moving_target;
  }
  if (attacker.has_component<Engine::Core::ElephantComponent>() &&
      has_formation_slots(target)) {
    return geometry.surface_gap <=
           -k_elephant_contact_penetration + k_contact_numeric_epsilon;
  }

  return geometry.surface_gap <= melee_reach(attacker) + k_contact_numeric_epsilon;
}

} // namespace Game::Systems::FormationCombat
