#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace Render::Ground {

struct BuildingGroundFootprint {
  float x = 0.0F;
  float z = 0.0F;
  float half_width = 3.0F;
  float half_depth = 3.0F;
  float yaw_degrees = 0.0F;
};

struct BuildingGroundSet {
  std::vector<BuildingGroundFootprint> footprints;
  std::uint64_t revision = 0U;
};

[[nodiscard]] inline auto next_building_ground_revision() -> std::uint64_t {
  static std::atomic<std::uint64_t> counter{0U};
  return counter.fetch_add(1U, std::memory_order_relaxed) + 1U;
}

template <typename World,
          typename BuildingTag,
          typename Transform,
          typename Unit,
          typename BodyOf>
void refresh_building_ground(World& world,
                             BuildingGroundSet& set,
                             std::uint64_t& signature,
                             BodyOf&& body_of) {
  std::uint64_t hash = 1469598103934665603ULL;
  auto mix = [&hash](std::int64_t value) {
    hash ^= static_cast<std::uint64_t>(value);
    hash *= 1099511628211ULL;
  };
  auto quantise = [](float value) {
    return static_cast<std::int64_t>(std::lround(value * 8.0F));
  };
  std::size_t count = 0U;
  for (auto [entity_id, building, transform] :
       world.template view<BuildingTag, Transform>()) {
    (void)building;
    mix(quantise(transform.position.x));
    mix(quantise(transform.position.z));
    mix(quantise(transform.rotation.y));
    const auto* unit = world.template try_get<Unit>(entity_id);
    mix(unit != nullptr ? static_cast<std::int64_t>(unit->spawn_type) + 1 : 0);
    ++count;
  }
  mix(static_cast<std::int64_t>(count));
  if (hash == signature && set.revision != 0U) {
    return;
  }
  signature = hash;

  std::vector<BuildingGroundFootprint> footprints;
  footprints.reserve(count);
  for (auto [entity_id, building, transform] :
       world.template view<BuildingTag, Transform>()) {
    (void)building;
    BuildingGroundFootprint footprint;
    footprint.x = transform.position.x;
    footprint.z = transform.position.z;
    footprint.yaw_degrees = transform.rotation.y;
    if (const auto* unit = world.template try_get<Unit>(entity_id); unit != nullptr) {
      auto const body = body_of(*unit, footprint.yaw_degrees);
      footprint.x += body.offset_x;
      footprint.z += body.offset_z;
      footprint.half_width = std::max(body.width * 0.5F, 0.75F);
      footprint.half_depth = std::max(body.depth * 0.5F, 0.75F);
    }
    footprints.push_back(footprint);
  }
  set.footprints = std::move(footprints);
  set.revision = next_building_ground_revision();
}

} // namespace Render::Ground
