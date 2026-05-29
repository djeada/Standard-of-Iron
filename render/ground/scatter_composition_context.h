#pragma once

#include <QVector2D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "../../game/map/map_definition.h"
#include "../../game/map/terrain_service.h"
#include "ground_utils.h"
#include "scatter_anchor_utils.h"
#include "spawn_validator.h"

namespace Render::Ground {

class ScatterCompositionContext {
public:
  ScatterCompositionContext(const SpawnTerrainCache& cache,
                            int width,
                            int height,
                            float tile_size,
                            const Game::Map::BiomeSettings& biome_settings,
                            const std::vector<Game::Map::WorldProp>& world_props = {})
      : m_cache(cache)
      , m_width(width)
      , m_height(height)
      , m_tile_size(tile_size)
      , m_half_grid_width(static_cast<float>(width) * 0.5F - 0.5F)
      , m_half_grid_height(static_cast<float>(height) * 0.5F - 0.5F)
      , m_biome_settings(biome_settings)
      , m_climate_profile(Game::Map::make_climate_profile(biome_settings)) {
    build_point_anchors(world_props);
    build_linear_anchors();
  }

  [[nodiscard]] auto sample_grid(float gx, float gz, std::uint32_t salt = 0U) const
      -> ScatterCompositionSample {
    float world_x = 0.0F;
    float world_z = 0.0F;
    grid_to_world(gx, gz, world_x, world_z);
    return sample_world(world_x, world_z, salt);
  }

  [[nodiscard]] auto sample_world(float world_x, float world_z, std::uint32_t salt = 0U)
      const -> ScatterCompositionSample {
    QVector2D const point(world_x, world_z);
    float gx = 0.0F;
    float gz = 0.0F;
    world_to_grid(world_x, world_z, gx, gz);
    int const grid_x = std::clamp(
        static_cast<int>(std::floor(gx + 0.5F)), 0, std::max(0, m_width - 1));
    int const grid_z = std::clamp(
        static_cast<int>(std::floor(gz + 0.5F)), 0, std::max(0, m_height - 1));

    ScatterCompositionSample sample;
    sample.terrain_type = m_cache.get_terrain_type_at(grid_x, grid_z);
    sample.slope = m_cache.get_slope_at(grid_x, grid_z);
    sample.camp_influence =
        Detail::point_influence(point, m_camp_anchors, 1.15F, 4.60F);
    sample.prop_influence =
        Detail::point_influence(point, m_structure_anchors, 0.55F, 3.80F);
    sample.obstacle_influence =
        Detail::point_influence(point, m_hard_obstacle_anchors, 1.0F, 1.5F);
    sample.river_influence =
        Detail::line_influence(point, m_river_anchors, 0.30F, 3.00F);
    sample.road_influence = Detail::line_influence(point, m_road_anchors, 0.60F, 5.20F);
    float const bridge_influence =
        Detail::line_influence(point, m_bridge_anchors, 0.40F, 3.60F);

    float const macro_noise = value_noise(
        world_x * 0.018F, world_z * 0.018F, m_biome_settings.seed ^ salt ^ 0x84A35F21U);
    float const micro_noise = value_noise(
        world_x * 0.043F, world_z * 0.043F, m_biome_settings.seed ^ salt ^ 0x1E4C9AB3U);
    float const ridge_noise = value_noise(
        world_x * 0.071F, world_z * 0.071F, m_biome_settings.seed ^ salt ^ 0x5C83D12FU);
    sample.cluster_bias = std::clamp(
        macro_noise * 0.55F + micro_noise * 0.30F + ridge_noise * 0.15F, 0.0F, 1.0F);

    float const terrain_rock_bonus =
        sample.terrain_type == Game::Map::TerrainType::Mountain
            ? 0.32F
            : (sample.terrain_type == Game::Map::TerrainType::Hill ? 0.14F : 0.0F);
    float const terrain_fertility_bonus =
        sample.terrain_type == Game::Map::TerrainType::Forest
            ? 0.18F
            : (sample.terrain_type == Game::Map::TerrainType::Flat ? 0.10F : 0.0F);

    sample.rockiness = std::clamp(
        m_climate_profile.rock_exposure * 0.55F + sample.slope * 0.85F +
            sample.prop_influence * 0.35F + terrain_rock_bonus + macro_noise * 0.22F -
            sample.river_influence * 0.20F + bridge_influence * 0.10F,
        0.0F,
        1.0F);
    sample.dryness = std::clamp(
        (1.0F - m_climate_profile.moisture_level) * 0.65F +
            m_climate_profile.crack_intensity * 0.55F + sample.slope * 0.35F +
            macro_noise * 0.18F + sample.road_influence * 0.08F -
            sample.river_influence * 0.50F - sample.camp_influence * 0.12F,
        0.0F,
        1.0F);
    sample.shelter = std::clamp(
        (1.0F - sample.slope) * 0.45F + sample.camp_influence * 0.18F +
            (sample.terrain_type == Game::Map::TerrainType::Forest ? 0.35F : 0.0F) +
            (1.0F - sample.cluster_bias) * 0.18F + bridge_influence * 0.05F,
        0.0F,
        1.0F);
    sample.fertility = std::clamp(
        m_climate_profile.moisture_level * 0.55F + (1.0F - sample.dryness) * 0.30F +
            sample.river_influence * 0.45F + sample.shelter * 0.25F +
            terrain_fertility_bonus - sample.rockiness * 0.25F,
        0.0F,
        1.0F);

    if (sample.camp_influence > 0.42F && sample.river_influence < 0.45F) {
      sample.archetype = ScatterSceneArchetype::CampOutskirts;
    } else if (sample.river_influence > 0.55F) {
      sample.archetype = ScatterSceneArchetype::RiverFringe;
    } else if (sample.rockiness > 0.62F || sample.prop_influence > 0.45F) {
      sample.archetype = ScatterSceneArchetype::RockyPatch;
    } else if (sample.dryness > 0.60F) {
      sample.archetype = ScatterSceneArchetype::DryClearing;
    } else if (sample.shelter > 0.58F && sample.fertility > 0.45F) {
      sample.archetype = ScatterSceneArchetype::ShelteredGrove;
    } else if (sample.fertility > 0.58F) {
      sample.archetype = ScatterSceneArchetype::FertilePatch;
    } else {
      sample.archetype = ScatterSceneArchetype::OpenField;
    }

    return sample;
  }

private:
  void build_point_anchors(const std::vector<Game::Map::WorldProp>& world_props) {
    float const half_anchor_width = static_cast<float>(m_width) * 0.5F - 0.5F;
    float const half_anchor_height = static_cast<float>(m_height) * 0.5F - 0.5F;

    for (const auto& world_prop : world_props) {
      float const world_x = (world_prop.x - half_anchor_width) * m_tile_size;
      float const world_z = (world_prop.z - half_anchor_height) * m_tile_size;
      if (world_prop.type == Game::Map::WorldProp::Type::FireCamp) {
        m_camp_anchors.push_back(
            {{world_x, world_z}, std::max(world_prop.radius, 1.5F)});
        continue;
      }
      if (world_prop.type == Game::Map::WorldProp::Type::PineTree ||
          world_prop.type == Game::Map::WorldProp::Type::OliveTree) {
        const float radius = std::max(1.5F, world_prop.scale * 2.5F);
        m_structure_anchors.push_back({{world_x, world_z}, radius});
        continue;
      }
      if (world_prop.type == Game::Map::WorldProp::Type::Ruins ||
          world_prop.type == Game::Map::WorldProp::Type::MagicShrine) {
        float const render_s = Game::Map::world_prop_render_scale(world_prop.type);
        float const obs_radius = std::max(2.0F, world_prop.scale * render_s);
        m_hard_obstacle_anchors.push_back({{world_x, world_z}, obs_radius});
      }
      if (world_prop.type != Game::Map::WorldProp::Type::Boulder &&
          world_prop.type != Game::Map::WorldProp::Type::Ruins &&
          world_prop.type != Game::Map::WorldProp::Type::IronOre) {
        continue;
      }
      float const radius = std::max(1.0F, world_prop.scale * 1.8F);
      m_structure_anchors.push_back({{world_x, world_z}, radius});
    }
  }

  void build_linear_anchors() {
    auto& terrain_service = Game::Map::TerrainService::instance();
    for (const auto& road : terrain_service.road_segments()) {
      m_road_anchors.push_back(
          {{road.start.x(), road.start.z()}, {road.end.x(), road.end.z()}, road.width});
    }

    auto const* height_map = terrain_service.get_height_map();
    if (height_map == nullptr) {
      return;
    }

    for (const auto& river : height_map->get_river_segments()) {
      m_river_anchors.push_back({{river.start.x(), river.start.z()},
                                 {river.end.x(), river.end.z()},
                                 river.width});
    }
    for (const auto& bridge : height_map->get_bridges()) {
      m_bridge_anchors.push_back({{bridge.start.x(), bridge.start.z()},
                                  {bridge.end.x(), bridge.end.z()},
                                  bridge.width});
    }
  }

  void grid_to_world(float gx, float gz, float& out_world_x, float& out_world_z) const {
    out_world_x = (gx - m_half_grid_width) * m_tile_size;
    out_world_z = (gz - m_half_grid_height) * m_tile_size;
  }

  void world_to_grid(float world_x, float world_z, float& out_gx, float& out_gz) const {
    out_gx = world_x / m_tile_size + m_half_grid_width;
    out_gz = world_z / m_tile_size + m_half_grid_height;
  }

  const SpawnTerrainCache& m_cache;
  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;
  float m_half_grid_width = 0.0F;
  float m_half_grid_height = 0.0F;
  Game::Map::BiomeSettings m_biome_settings;
  Game::Map::ClimateProfile m_climate_profile;
  std::vector<Detail::PointAnchor> m_camp_anchors;
  std::vector<Detail::PointAnchor> m_structure_anchors;
  std::vector<Detail::PointAnchor> m_hard_obstacle_anchors;
  std::vector<Detail::LinearAnchor> m_road_anchors;
  std::vector<Detail::LinearAnchor> m_river_anchors;
  std::vector<Detail::LinearAnchor> m_bridge_anchors;
};

} // namespace Render::Ground
