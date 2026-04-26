#pragma once

#include "../game/map/terrain_service.h"
#include "ground/biome_renderer.h"
#include "ground/bridge_renderer.h"
#include "ground/firecamp_renderer.h"
#include "ground/fog_renderer.h"
#include "ground/ground_renderer.h"
#include "ground/olive_renderer.h"
#include "ground/pine_renderer.h"
#include "ground/plant_renderer.h"
#include "ground/rain_renderer.h"
#include "ground/river_renderer.h"
#include "ground/riverbank_renderer.h"
#include "ground/road_renderer.h"
#include "ground/stone_renderer.h"
#include "ground/terrain_renderer.h"
#include "terrain_scene_types.h"
#include <vector>

namespace Render::GL {

class Renderer;
class ResourceManager;

class TerrainSceneProxy {
public:
  TerrainSceneProxy(GroundRenderer *ground, TerrainRenderer *terrain,
                    RiverRenderer *river, RoadRenderer *road,
                    RiverbankRenderer *riverbank, BridgeRenderer *bridge,
                    BiomeRenderer *biome, StoneRenderer *stone,
                    PlantRenderer *plant, PineRenderer *pine,
                    OliveRenderer *olive, FireCampRenderer *firecamp,
                    RainRenderer *rain, FogRenderer *fog)
      : m_ground(ground), m_terrain(terrain), m_river(river), m_road(road),
        m_riverbank(riverbank), m_bridge(bridge), m_biome(biome),
        m_stone(stone), m_plant(plant), m_pine(pine), m_olive(olive),
        m_firecamp(firecamp), m_rain(rain), m_fog(fog),
        m_passes{ground, terrain, river, road, riverbank, bridge, biome, stone,
                 plant, pine, olive, firecamp, rain, fog} {}

  // Submits each terrain pass through the shared terrain scene boundary.
  // Null entries are tolerated so legacy bootstrap or tests can omit passes
  // while preserving the established terrain submission order.
  void submit(Renderer &renderer, ResourceManager *resources) const {
    submit_surfaces(renderer, resources);
    submit_features(renderer, resources);
    submit_scatters(renderer, resources);

    for (auto *pass : {static_cast<IRenderPass *>(m_rain),
                       static_cast<IRenderPass *>(m_fog)}) {
      if (pass != nullptr) {
        pass->submit(renderer, resources);
      }
    }
  }

  [[nodiscard]] auto ground() const -> GroundRenderer * { return m_ground; }
  [[nodiscard]] auto terrain() const -> TerrainRenderer * { return m_terrain; }
  [[nodiscard]] auto river() const -> RiverRenderer * { return m_river; }
  [[nodiscard]] auto road() const -> RoadRenderer * { return m_road; }
  [[nodiscard]] auto riverbank() const -> RiverbankRenderer * {
    return m_riverbank;
  }
  [[nodiscard]] auto bridge() const -> BridgeRenderer * { return m_bridge; }
  [[nodiscard]] auto biome() const -> BiomeRenderer * { return m_biome; }
  [[nodiscard]] auto stone() const -> StoneRenderer * { return m_stone; }
  [[nodiscard]] auto plant() const -> PlantRenderer * { return m_plant; }
  [[nodiscard]] auto pine() const -> PineRenderer * { return m_pine; }
  [[nodiscard]] auto olive() const -> OliveRenderer * { return m_olive; }
  [[nodiscard]] auto firecamp() const -> FireCampRenderer * {
    return m_firecamp;
  }
  [[nodiscard]] auto rain() const -> RainRenderer * { return m_rain; }
  [[nodiscard]] auto fog() const -> FogRenderer * { return m_fog; }

  [[nodiscard]] auto has_field() const -> bool {
    return Game::Map::TerrainService::instance().is_initialized();
  }

  [[nodiscard]] auto field() const -> const Game::Map::TerrainField & {
    return Game::Map::TerrainService::instance().terrain_field();
  }

  [[nodiscard]] auto road_segments() const
      -> const std::vector<Game::Map::RoadSegment> & {
    return Game::Map::TerrainService::instance().road_segments();
  }

  [[nodiscard]] auto surface_chunks() const -> std::vector<TerrainSurfaceChunk> {
    auto &terrain = Game::Map::TerrainService::instance();
    TerrainSurfaceShaderParams base_params;
    if (terrain.is_initialized()) {
      base_params.biome_settings = &terrain.biome_settings();
      base_params.field = &terrain.terrain_field();
    }

    return {{TerrainSurfaceKind::GroundPlane, m_ground,
             TerrainSurfaceShaderParams{base_params.biome_settings,
                                        base_params.field, true}},
            {TerrainSurfaceKind::TerrainMesh, m_terrain,
             TerrainSurfaceShaderParams{base_params.biome_settings,
                                        base_params.field, false}}};
  }

  [[nodiscard]] auto feature_chunks() const -> std::vector<LinearFeatureChunk> {
    auto &terrain = Game::Map::TerrainService::instance();
    auto const *height_map = terrain.get_height_map();
    size_t const river_count =
        (height_map != nullptr) ? height_map->getRiverSegments().size() : 0U;
    size_t const bridge_count =
        (height_map != nullptr) ? height_map->getBridges().size() : 0U;

    return {{LinearFeatureKind::River,
             LinearFeatureVisibilityMode::SegmentSampled, m_river, river_count},
            {LinearFeatureKind::Road,
             LinearFeatureVisibilityMode::SegmentSampled, m_road,
             road_segments().size()},
            {LinearFeatureKind::Riverbank,
             LinearFeatureVisibilityMode::TextureDriven, m_riverbank,
             river_count},
            {LinearFeatureKind::Bridge,
             LinearFeatureVisibilityMode::SegmentSampled, m_bridge,
             bridge_count}};
  }

  [[nodiscard]] auto scatter_chunks() const -> std::vector<ScatterChunk> {
    return {{ScatterSpeciesId::Grass, ScatterVisibilityMode::None, m_biome,
             m_biome != nullptr ? m_biome->instance_count() : 0U,
             m_biome == nullptr || m_biome->is_gpu_ready()},
            {ScatterSpeciesId::Stone, ScatterVisibilityMode::None, m_stone,
             m_stone != nullptr ? m_stone->instance_count() : 0U,
             m_stone == nullptr || m_stone->is_gpu_ready()},
            {ScatterSpeciesId::Plant, ScatterVisibilityMode::InstanceFiltered,
             m_plant, m_plant != nullptr ? m_plant->instance_count() : 0U,
             m_plant == nullptr || m_plant->is_gpu_ready()},
            {ScatterSpeciesId::Pine, ScatterVisibilityMode::InstanceFiltered,
             m_pine, m_pine != nullptr ? m_pine->instance_count() : 0U,
             m_pine == nullptr || m_pine->is_gpu_ready()},
            {ScatterSpeciesId::Olive, ScatterVisibilityMode::InstanceFiltered,
             m_olive, m_olive != nullptr ? m_olive->instance_count() : 0U,
             m_olive == nullptr || m_olive->is_gpu_ready()},
            {ScatterSpeciesId::FireCamp, ScatterVisibilityMode::InstanceFiltered,
             m_firecamp,
             m_firecamp != nullptr ? m_firecamp->instance_count() : 0U,
             m_firecamp == nullptr || m_firecamp->is_gpu_ready()}};
  }

  // Exposes the ordered pass list for focused tests and adapter code that
  // still needs to inspect the legacy terrain pass sequence directly.
  [[nodiscard]] auto passes() const -> const std::vector<IRenderPass *> & {
    return m_passes;
  }

private:
  void submit_surfaces(Renderer &renderer, ResourceManager *resources) const {
    for (const auto &chunk : surface_chunks()) {
      if (chunk.pass != nullptr) {
        chunk.pass->submit(renderer, resources);
      }
    }
  }

  void submit_features(Renderer &renderer, ResourceManager *resources) const {
    for (const auto &chunk : feature_chunks()) {
      if (chunk.pass != nullptr) {
        chunk.pass->submit(renderer, resources);
      }
    }
  }

  void submit_scatters(Renderer &renderer, ResourceManager *resources) const {
    for (const auto &chunk : scatter_chunks()) {
      if (chunk.pass != nullptr) {
        chunk.pass->submit(renderer, resources);
      }
    }
  }

  GroundRenderer *m_ground = nullptr;
  TerrainRenderer *m_terrain = nullptr;
  RiverRenderer *m_river = nullptr;
  RoadRenderer *m_road = nullptr;
  RiverbankRenderer *m_riverbank = nullptr;
  BridgeRenderer *m_bridge = nullptr;
  BiomeRenderer *m_biome = nullptr;
  StoneRenderer *m_stone = nullptr;
  PlantRenderer *m_plant = nullptr;
  PineRenderer *m_pine = nullptr;
  OliveRenderer *m_olive = nullptr;
  FireCampRenderer *m_firecamp = nullptr;
  RainRenderer *m_rain = nullptr;
  FogRenderer *m_fog = nullptr;
  std::vector<IRenderPass *> m_passes;
};

} // namespace Render::GL
