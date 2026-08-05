#include "wildlife_placement.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "../map/map_definition.h"

namespace Game::Wildlife {

namespace {

constexpr float k_edge_margin = 6.0F;
constexpr float k_structure_keep_out = 24.0F;
constexpr float k_spawn_keep_out = 15.0F;
constexpr float k_undead_keep_out_padding = 6.0F;
constexpr float k_water_padding = 3.5F;
constexpr float k_road_padding = 2.5F;
constexpr float k_lattice_divisions = 14.0F;
constexpr float k_min_lattice_step = 6.0F;
constexpr float k_separation_factor = 1.7F;
constexpr float k_cover_probe_radius = 26.0F;

constexpr float k_sheep_pasture_ideal_cover = 0.28F;
constexpr float k_bird_cover_weight = 0.35F;

constexpr float k_area_per_sheep_herd = 3400.0F;
constexpr float k_area_per_wolf_pack = 9000.0F;

constexpr float k_area_per_bird_flock = 26000.0F;

struct Stage {
  float min_x{0.0F};
  float max_x{0.0F};
  float min_z{0.0F};
  float max_z{0.0F};

  [[nodiscard]] auto width() const -> float { return max_x - min_x; }
  [[nodiscard]] auto depth() const -> float { return max_z - min_z; }
  [[nodiscard]] auto area() const -> float { return width() * depth(); }
};

struct Candidate {
  float x{0.0F};
  float z{0.0F};
  float score{0.0F};
};

auto next_random(std::uint32_t& state) -> float {
  state = (state * 1664525U) + 1013904223U;
  return static_cast<float>((state >> 8U) & 0xFFFFFFU) / 16777216.0F;
}

auto stage_of(const Game::Map::MapDefinition& map) -> Stage {
  float const tile = std::max(0.0001F, map.grid.tile_size);
  float const half_x = ((static_cast<float>(map.grid.width) * 0.5F) - 0.5F) * tile;
  float const half_z = ((static_cast<float>(map.grid.height) * 0.5F) - 0.5F) * tile;
  float const margin_x = std::min(k_edge_margin, half_x * 0.4F);
  float const margin_z = std::min(k_edge_margin, half_z * 0.4F);
  return {-half_x + margin_x, half_x - margin_x, -half_z + margin_z, half_z - margin_z};
}

auto distance_to_segment(float px, float pz, float ax, float az, float bx, float bz)
    -> float {
  float const dx = bx - ax;
  float const dz = bz - az;
  float const length_sq = (dx * dx) + (dz * dz);
  float t = 0.0F;
  if (length_sq > 0.0001F) {
    t = std::clamp((((px - ax) * dx) + ((pz - az) * dz)) / length_sq, 0.0F, 1.0F);
  }
  float const cx = ax + (dx * t);
  float const cz = az + (dz * t);
  return std::hypot(px - cx, pz - cz);
}

auto structure_position(const Game::Map::StructureEntry& entry)
    -> std::pair<float, float> {
  if (const auto* point =
          std::get_if<Game::Map::PointStructureGeometry>(&entry.geometry)) {
    return {point->position.x(), point->position.z()};
  }
  if (const auto* line =
          std::get_if<Game::Map::LineStructureGeometry>(&entry.geometry)) {
    return {(line->start.x() + line->end.x()) * 0.5F,
            (line->start.z() + line->end.z()) * 0.5F};
  }
  return {0.0F, 0.0F};
}

class Terrain {
public:
  explicit Terrain(const Game::Map::MapDefinition& map)
      : m_map(map) {}

  [[nodiscard]] auto is_wet(float x, float z) const -> bool {
    for (const auto& lake : m_map.lakes) {
      if (Game::Map::point_in_lake(lake, x, z, k_water_padding)) {
        return true;
      }
    }
    for (const auto& river : m_map.rivers) {
      float const reach = (river.width * 0.5F) + k_water_padding;
      if (distance_to_segment(
              x, z, river.start.x(), river.start.z(), river.end.x(), river.end.z()) <=
          reach) {
        return true;
      }
    }
    for (const auto& feature : m_map.terrain) {
      if (!Game::Map::is_water_terrain(feature.type)) {
        continue;
      }
      if (std::hypot(x - feature.center_x, z - feature.center_z) <=
          feature.radius + k_water_padding) {
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] auto is_impassable(float x, float z) const -> bool {
    for (const auto& feature : m_map.terrain) {
      if (feature.type != Game::Map::TerrainType::Mountain) {
        continue;
      }
      if (std::hypot(x - feature.center_x, z - feature.center_z) <= feature.radius) {
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] auto is_paved(float x, float z) const -> bool {
    for (const auto& road : m_map.roads) {
      float const reach = (road.width * 0.5F) + k_road_padding;
      if (distance_to_segment(
              x, z, road.start.x(), road.start.z(), road.end.x(), road.end.z()) <=
          reach) {
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] auto cover(float x, float z) const -> float {
    float best = 0.0F;
    for (const auto& feature : m_map.terrain) {
      if (feature.type != Game::Map::TerrainType::Forest) {
        continue;
      }
      float const distance = std::hypot(x - feature.center_x, z - feature.center_z);
      float const reach = feature.radius + k_cover_probe_radius;
      if (distance >= reach) {
        continue;
      }
      best = std::max(best, 1.0F - (distance / reach));
    }

    int nearby_trees = 0;
    for (const auto& prop : m_map.world_props) {
      if (!Game::Map::is_tree_world_prop_type(prop.type)) {
        continue;
      }
      if (std::hypot(x - prop.x, z - prop.z) <= k_cover_probe_radius) {
        ++nearby_trees;
      }
    }
    float const tree_cover = std::min(1.0F, static_cast<float>(nearby_trees) / 12.0F);
    return std::max(best, tree_cover);
  }

  [[nodiscard]] auto relief(float x, float z) const -> float {
    float best = 0.0F;
    for (const auto& feature : m_map.terrain) {
      if (feature.type != Game::Map::TerrainType::Hill) {
        continue;
      }
      float const distance = std::hypot(x - feature.center_x, z - feature.center_z);
      if (distance >= feature.radius) {
        continue;
      }
      best = std::max(best, 1.0F - (distance / std::max(1.0F, feature.radius)));
    }
    return best;
  }

  [[nodiscard]] auto distance_to_settlement(float x, float z) const -> float {
    float nearest = std::numeric_limits<float>::max();
    for (const auto& structure : m_map.structures) {
      auto const [sx, sz] = structure_position(structure);
      nearest = std::min(nearest, std::hypot(x - sx, z - sz));
    }
    for (const auto& spawn : m_map.spawns) {
      nearest = std::min(nearest, std::hypot(x - spawn.x, z - spawn.z));
    }
    return nearest;
  }

  [[nodiscard]] auto is_claimed(float x, float z) const -> bool {
    for (const auto& structure : m_map.structures) {
      auto const [sx, sz] = structure_position(structure);
      if (std::hypot(x - sx, z - sz) <= k_structure_keep_out) {
        return true;
      }
    }
    for (const auto& spawn : m_map.spawns) {
      if (std::hypot(x - spawn.x, z - spawn.z) <= k_spawn_keep_out) {
        return true;
      }
    }
    for (const auto& zone : m_map.undead_zones) {
      float const reach =
          std::max(zone.radius, zone.leash_radius) + k_undead_keep_out_padding;
      if (std::hypot(x - zone.x, z - zone.z) <= reach) {
        return true;
      }
    }
    return false;
  }

private:
  const Game::Map::MapDefinition& m_map;
};

auto score_for(Species species,
               const Terrain& terrain,
               float x,
               float z,
               float settlement_distance) -> float {

  float const cover = terrain.cover(x, z);
  float const relief = terrain.relief(x, z);
  float const room = std::min(1.0F, settlement_distance / 90.0F);

  switch (species) {
  case Species::Sheep: {

    float const pasture = 1.0F - std::abs(cover - k_sheep_pasture_ideal_cover) * 1.6F;
    return (std::max(0.0F, pasture) * 1.0F) + (room * 0.8F) - (relief * 0.5F);
  }
  case Species::Wolf:

    return (cover * 1.1F) + (relief * 0.45F) + (room * 1.2F);
  case Species::Bird:

    return (cover * k_bird_cover_weight) + (room * 0.6F);
  }
  return room;
}

auto gather_candidates(Species species,
                       const Terrain& terrain,
                       const Stage& stage,
                       std::uint32_t& rng) -> std::vector<Candidate> {
  float const step = std::max(
      k_min_lattice_step, std::min(stage.width(), stage.depth()) / k_lattice_divisions);

  std::vector<Candidate> candidates;
  for (float z = stage.min_z; z <= stage.max_z; z += step) {
    for (float x = stage.min_x; x <= stage.max_x; x += step) {
      float const jitter_x = (next_random(rng) - 0.5F) * step * 0.8F;
      float const jitter_z = (next_random(rng) - 0.5F) * step * 0.8F;
      float const px = std::clamp(x + jitter_x, stage.min_x, stage.max_x);
      float const pz = std::clamp(z + jitter_z, stage.min_z, stage.max_z);

      if (terrain.is_wet(px, pz) || terrain.is_impassable(px, pz) ||
          terrain.is_paved(px, pz) || terrain.is_claimed(px, pz)) {
        continue;
      }

      float const settlement_distance = terrain.distance_to_settlement(px, pz);
      Candidate candidate;
      candidate.x = px;
      candidate.z = pz;
      candidate.score = score_for(species, terrain, px, pz, settlement_distance) +
                        (next_random(rng) * 0.12F);
      candidates.push_back(candidate);
    }
  }

  std::sort(
      candidates.begin(),
      candidates.end(),
      [](const Candidate& lhs, const Candidate& rhs) { return lhs.score > rhs.score; });
  return candidates;
}

void assign_spawn_areas(Species species,
                        const Terrain& terrain,
                        const Stage& stage,
                        std::uint32_t seed,
                        SpeciesConfig& config) {
  if (!config.enabled || config.group_count <= 0 || !config.spawn_areas.empty()) {
    return;
  }

  std::uint32_t rng = seed | 1U;
  auto const candidates = gather_candidates(species, terrain, stage, rng);
  if (candidates.empty()) {

    config.spawn_areas.push_back(
        {(stage.min_x + stage.max_x) * 0.5F,
         (stage.min_z + stage.max_z) * 0.5F,
         std::max(config.roam_radius, std::min(stage.width(), stage.depth()) * 0.30F)});
    return;
  }

  float const separation = config.roam_radius * k_separation_factor;
  for (const auto& candidate : candidates) {
    if (static_cast<int>(config.spawn_areas.size()) >= config.group_count) {
      break;
    }
    bool crowded = false;
    for (const auto& placed : config.spawn_areas) {
      if (std::hypot(candidate.x - placed.x, candidate.z - placed.z) < separation) {
        crowded = true;
        break;
      }
    }
    if (crowded) {
      continue;
    }
    config.spawn_areas.push_back({candidate.x, candidate.z, config.roam_radius});
  }

  if (config.spawn_areas.empty()) {
    config.spawn_areas.push_back(
        {candidates.front().x, candidates.front().z, config.roam_radius});
  }
}

auto groups_for_area(float area, float area_per_group, int cap) -> int {
  auto const groups = static_cast<int>(std::lround(area / area_per_group));
  return std::clamp(groups, 1, cap);
}

} // namespace

auto default_settings_for_map(const Game::Map::MapDefinition& map) -> WildlifeSettings {
  WildlifeSettings settings = default_settings();
  settings.enabled = true;
  settings.seed = map.biome.seed ^ 0x57EE9AU;

  Stage const stage = stage_of(map);
  float const area = std::max(1.0F, stage.area());

  settings.sheep.group_count = groups_for_area(area, k_area_per_sheep_herd, 4);
  settings.wolves.group_count = groups_for_area(area, k_area_per_wolf_pack, 2);
  settings.birds.group_count = groups_for_area(area, k_area_per_bird_flock, 2);

  sanitize(settings);
  return settings;
}

void populate_missing_spawn_areas(const Game::Map::MapDefinition& map,
                                  WildlifeSettings& settings) {
  if (!settings.enabled) {
    return;
  }

  Terrain const terrain(map);
  Stage const stage = stage_of(map);
  std::uint32_t const seed =
      settings.seed != 0U ? settings.seed : (map.biome.seed | 1U);

  constexpr std::array<Species, 3> k_species{
      Species::Sheep, Species::Wolf, Species::Bird};
  for (Species const species : k_species) {
    auto& config = settings.for_species(species);
    assign_spawn_areas(
        species,
        terrain,
        stage,
        seed ^
            ((static_cast<std::uint32_t>(species_index(species)) + 1U) * 2654435761U),
        config);
  }
}

} // namespace Game::Wildlife
