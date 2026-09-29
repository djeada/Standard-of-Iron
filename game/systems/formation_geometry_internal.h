#pragma once

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <unordered_map>
#include <vector>

#include "../core/component.h"
#include "../core/entity.h"
#include "../core/registry.h"
#include "formation_combat_geometry.h"

namespace Game::Systems::FormationCombat::Detail {

inline constexpr float k_elephant_visual_body_radius = 1.15F;

inline void hash_combine(std::uint64_t& seed, std::uint64_t value) noexcept {
  seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
}

inline void hash_float(std::uint64_t& seed, float value) noexcept {
  hash_combine(seed, std::bit_cast<std::uint32_t>(value));
}

struct FormationCacheKey {
  std::uint64_t registry{0};
  Engine::Core::EntityID entity{0};

  auto operator==(const FormationCacheKey&) const -> bool = default;
};

struct FormationCacheKeyHash {
  auto operator()(const FormationCacheKey& key) const noexcept -> std::size_t {
    std::uint64_t seed = 0xcbf29ce484222325ULL;
    seed ^= key.registry + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
    seed ^= key.entity + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
    return static_cast<std::size_t>(seed);
  }
};

[[nodiscard]] inline auto
cache_key(const Engine::Core::Entity& entity) -> FormationCacheKey {
  const auto* registry = entity.registry();
  return {.registry = registry != nullptr ? registry->instance_id() : 0U,
          .entity = entity.get_id()};
}

[[nodiscard]] auto formation_cache_epoch() -> std::uint64_t;

template <typename Map>
void evict_incrementally(Map& cache, std::size_t limit) {
  if (cache.size() <= limit) {
    return;
  }
  const std::uint64_t epoch = formation_cache_epoch();
  std::size_t budget = 64U;
  for (auto it = cache.begin(); it != cache.end() && budget > 0U; --budget) {
    it = it->second.epoch != epoch ? cache.erase(it) : std::next(it);
  }
  if (cache.size() > limit + limit / 2U) {
    cache.clear();
  }
}

struct LayoutRevisions {
  std::uint32_t traversal{0U};
  std::uint32_t presentation{0U};
  bool has_traversal{false};
  bool has_presentation{false};

  auto operator==(const LayoutRevisions&) const -> bool = default;
};

struct LayoutCacheEntry {
  std::uint64_t epoch{0};
  std::uint64_t local_signature{0};
  float world_x{0.0F};
  float world_z{0.0F};
  float yaw{0.0F};
  float turn_radius{0.0F};
  FormationExtents extents;
  FormationLayout layout;
};

struct SpatialLayoutCacheEntry {
  std::uint64_t epoch{0};
  std::uint64_t base_signature{0};
  float world_x{0.0F};
  float world_z{0.0F};
  float yaw{0.0F};
  LayoutRevisions revisions;
  bool valid{false};
  FormationLayout layout;
};

struct SlotsCacheEntry {
  std::uint64_t epoch{0};
  std::uint64_t signature{0};
  bool has_slots{false};
  bool valid{false};
};

struct ContactCacheKey {
  FormationCacheKey attacker;
  FormationCacheKey target;

  auto operator==(const ContactCacheKey&) const -> bool = default;
};

struct ContactCacheKeyHash {
  auto operator()(const ContactCacheKey& key) const noexcept -> std::size_t {
    std::uint64_t seed = 0xcbf29ce484222325ULL;
    hash_combine(seed, key.attacker.registry);
    hash_combine(seed, key.attacker.entity);
    hash_combine(seed, key.target.registry);
    hash_combine(seed, key.target.entity);
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
  std::uint64_t epoch{0};
  ContactCacheStamp stamp;
  ContactGeometry geometry;
  bool valid{false};
};

struct SlotOffset {
  float world_x{0.0F};
  float world_z{0.0F};
  float offset_x{0.0F};
  float offset_z{0.0F};
};

struct ContactSlotsEntry {
  std::uint64_t epoch{0};
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

inline constexpr std::size_t k_max_cached_layouts = 8192U;
inline constexpr std::size_t k_max_cached_contacts = 65536U;

struct GeometryCaches {
  std::unordered_map<FormationCacheKey, LayoutCacheEntry, FormationCacheKeyHash> layout;
  std::uint64_t layout_generation{0};
  std::unordered_map<FormationCacheKey, SpatialLayoutCacheEntry, FormationCacheKeyHash>
      spatial;
  std::unordered_map<FormationCacheKey, SlotsCacheEntry, FormationCacheKeyHash>
      slot_presence;
  std::unordered_map<ContactCacheKey, ContactCacheEntry, ContactCacheKeyHash> contact;
  std::unordered_map<FormationCacheKey, ContactSlotsEntry, FormationCacheKeyHash>
      contact_slots;
  ContactStats stats;

  void clear();
};

[[nodiscard]] inline auto caches() -> GeometryCaches& {
  thread_local GeometryCaches instance;
  return instance;
}

struct WorkSite {
  bool active{false};
  float half_width{0.0F};
  float half_depth{0.0F};
  float relative_yaw_radians{0.0F};
};

[[nodiscard]] auto
work_site_for(const Engine::Core::Entity& entity,
              const Engine::Core::TransformComponent& transform) -> WorkSite;

void place_on_site_perimeter(const WorkSite& site, float& offset_x, float& offset_z);

[[nodiscard]] auto
holds_formation_line(const Engine::Core::Entity& entity) noexcept -> bool;

[[nodiscard]] auto
definition_total_count(const Engine::Core::UnitComponent& unit) -> int;

[[nodiscard]] auto
layout_signature(const Engine::Core::Entity& entity) -> std::uint64_t;

[[nodiscard]] auto
compute_formation_extents(const FormationLayout& layout) -> FormationExtents;

[[nodiscard]] auto
resolve_layout_entry(const Engine::Core::Entity& entity) -> const LayoutCacheEntry*;

[[nodiscard]] auto
resolve_layout_entry(const Engine::Core::Entity& entity,
                     std::uint64_t signature) -> const LayoutCacheEntry*;

[[nodiscard]] auto
layout_revisions_of(const Engine::Core::Entity& entity) noexcept -> LayoutRevisions;

void spatialize_layout_into(const Engine::Core::Entity& entity,
                            const FormationLayout& base_layout,
                            FormationLayout& result);

[[nodiscard]] auto
spatialized_layout_for(const Engine::Core::Entity& entity,
                       const FormationLayout& base_layout,
                       std::uint64_t base_signature) -> const FormationLayout&;

} // namespace Game::Systems::FormationCombat::Detail
