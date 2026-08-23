#pragma once

#include <vector>

#include "game/map/terrain_service.h"
#include "ground/ambient_fog_renderer.h"
#include "ground/biome_renderer.h"
#include "ground/bridge_renderer.h"
#include "ground/firecamp_renderer.h"
#include "ground/fog_renderer.h"
#include "ground/ground_renderer.h"
#include "ground/map_boundary_fog_renderer.h"
#include "ground/plant_renderer.h"
#include "ground/rain_renderer.h"
#include "ground/river_renderer.h"
#include "ground/riverbank_renderer.h"
#include "ground/road_renderer.h"
#include "ground/stone_renderer.h"
#include "ground/terrain_feature_manager.h"
#include "ground/terrain_renderer.h"
#include "ground/terrain_scatter_manager.h"
#include "ground/terrain_surface_manager.h"
#include "ground/tree_renderer.h"
#include "scene_renderer.h"
#include "terrain_scene_types.h"

namespace Render::GL {

class ResourceManager;

struct TerrainSceneSubmitOptions {
  bool include_scatters = true;
  bool include_features = true;
  bool include_environment = true;
};

class TerrainSceneProxy {
public:
  TerrainSceneProxy(TerrainSurfaceManager* surface,
                    TerrainFeatureManager* features,
                    TerrainScatterManager* scatter,
                    RainRenderer* rain,
                    FogRenderer* fog,
                    MapBoundaryFogRenderer* boundary_fog,
                    AmbientFogRenderer* ambient_fog = nullptr)
      : m_surface(surface)
      , m_features(features)
      , m_scatter(scatter)
      , m_rain(rain)
      , m_fog(fog)
      , m_boundary_fog(boundary_fog)
      , m_ambient_fog(ambient_fog) {
    if (m_surface != nullptr) {
      const auto& surface_passes = m_surface->passes();
      m_passes.insert(m_passes.end(), surface_passes.begin(), surface_passes.end());
    }
    if (m_features != nullptr) {
      const auto& feature_passes = m_features->passes();
      m_passes.insert(m_passes.end(), feature_passes.begin(), feature_passes.end());
    }
    if (m_scatter != nullptr) {
      const auto& scatter_passes = m_scatter->passes();
      m_passes.insert(m_passes.end(), scatter_passes.begin(), scatter_passes.end());
    }
    m_passes.push_back(rain);
    m_passes.push_back(fog);
    m_passes.push_back(boundary_fog);
    m_passes.push_back(ambient_fog);
  }

  void set_world_view(const Render::WorldView& view) const {
    m_world_view = view;
    for (auto* pass : m_passes) {
      if (pass != nullptr) {
        pass->set_world_view(view);
      }
    }
  }

  void submit(Renderer& renderer,
              ResourceManager* resources,
              TerrainSceneSubmitOptions options = {}) const {
    set_world_view(renderer.world_view());
    submit_surfaces(renderer, resources);
    if (options.include_features) {
      submit_features(renderer, resources);
    }
    if (options.include_scatters) {
      submit_scatters(renderer, resources);
    }

    if (!options.include_environment) {
      return;
    }
    for (auto* pass : {static_cast<IRenderPass*>(m_rain),
                       static_cast<IRenderPass*>(m_fog),
                       static_cast<IRenderPass*>(m_boundary_fog),
                       static_cast<IRenderPass*>(m_ambient_fog)}) {
      if (pass != nullptr) {
        pass->submit(renderer, resources);
      }
    }
  }

  [[nodiscard]] auto surface() const -> TerrainSurfaceManager* { return m_surface; }
  [[nodiscard]] auto ground() const -> GroundRenderer* {
    return m_surface != nullptr ? m_surface->ground() : nullptr;
  }
  [[nodiscard]] auto terrain() const -> TerrainRenderer* {
    return m_surface != nullptr ? m_surface->terrain() : nullptr;
  }
  [[nodiscard]] auto water() const -> WaterRenderer* {
    return m_features != nullptr ? m_features->water() : nullptr;
  }
  [[nodiscard]] auto road() const -> RoadRenderer* {
    return m_features != nullptr ? m_features->road() : nullptr;
  }
  [[nodiscard]] auto shoreline() const -> ShorelineRenderer* {
    return m_features != nullptr ? m_features->shoreline() : nullptr;
  }
  [[nodiscard]] auto bridge() const -> BridgeRenderer* {
    return m_features != nullptr ? m_features->bridge() : nullptr;
  }
  [[nodiscard]] auto biome() const -> BiomeRenderer* {
    return m_scatter != nullptr ? m_scatter->biome() : nullptr;
  }
  [[nodiscard]] auto stone() const -> StoneRenderer* {
    return m_scatter != nullptr ? m_scatter->stone() : nullptr;
  }
  [[nodiscard]] auto plant() const -> PlantRenderer* {
    return m_scatter != nullptr ? m_scatter->plant() : nullptr;
  }
  [[nodiscard]] auto tree(Game::Map::TreeSpecies species) const -> TreeRenderer* {
    return m_scatter != nullptr ? m_scatter->tree(species) : nullptr;
  }
  [[nodiscard]] auto firecamp() const -> FireCampRenderer* {
    return m_scatter != nullptr ? m_scatter->firecamp() : nullptr;
  }
  [[nodiscard]] auto rain() const -> RainRenderer* { return m_rain; }
  [[nodiscard]] auto fog() const -> FogRenderer* { return m_fog; }
  [[nodiscard]] auto boundary_fog() const -> MapBoundaryFogRenderer* {
    return m_boundary_fog;
  }
  [[nodiscard]] auto ambient_fog() const -> AmbientFogRenderer* {
    return m_ambient_fog;
  }

  [[nodiscard]] auto has_field() const -> bool { return m_world_view.has_terrain(); }

  [[nodiscard]] auto field() const -> const Game::Map::TerrainField& {
    return m_world_view.terrain_or_empty().terrain_field();
  }

  [[nodiscard]] auto
  road_segments() const -> const std::vector<Game::Map::RoadSegment>& {
    return m_world_view.terrain_or_empty().road_segments();
  }

  [[nodiscard]] auto surface_chunks() const -> std::vector<TerrainSurfaceChunk> {
    const auto& terrain_service = m_world_view.terrain_or_empty();
    TerrainSurfaceShaderParams base_params;
    if (terrain_service.is_initialized()) {
      base_params.biome_settings = &terrain_service.biome_settings();
      base_params.field = &terrain_service.terrain_field();
    }

    return {{TerrainSurfaceKind::GroundPlane,
             ground(),
             TerrainSurfaceShaderParams{
                 base_params.biome_settings, base_params.field, true}},
            {TerrainSurfaceKind::TerrainMesh,
             terrain(),
             TerrainSurfaceShaderParams{
                 base_params.biome_settings, base_params.field, false}}};
  }

  [[nodiscard]] auto feature_chunks() const -> std::vector<LinearFeatureChunk> {
    const auto& terrain = m_world_view.terrain_or_empty();
    auto const* height_map = terrain.get_height_map();
    std::size_t const water_count =
        height_map != nullptr
            ? height_map->get_river_segments().size() + height_map->get_lakes().size()
            : 0U;
    std::size_t const bridge_count =
        (height_map != nullptr) ? height_map->get_bridges().size() : 0U;
    if (m_features == nullptr) {
      return {};
    }

    return m_features->chunks(water_count, road_segments().size(), bridge_count);
  }

  [[nodiscard]] auto scatter_chunks() const -> std::vector<ScatterChunk> {
    if (m_scatter == nullptr) {
      return {};
    }

    return m_scatter->chunks();
  }

  [[nodiscard]] auto passes() const -> const std::vector<IRenderPass*>& {
    return m_passes;
  }

private:
  void submit_surfaces(Renderer& renderer, ResourceManager* resources) const {
    if (m_surface != nullptr) {
      m_surface->submit(renderer, resources);
    }
  }

  void submit_features(Renderer& renderer, ResourceManager* resources) const {
    if (m_features != nullptr) {
      m_features->submit(renderer, resources);
    }
  }

  void submit_scatters(Renderer& renderer, ResourceManager* resources) const {
    if (m_scatter != nullptr) {
      m_scatter->submit(renderer, resources);
    }
  }

  TerrainSurfaceManager* m_surface = nullptr;
  TerrainFeatureManager* m_features = nullptr;
  TerrainScatterManager* m_scatter = nullptr;
  RainRenderer* m_rain = nullptr;
  FogRenderer* m_fog = nullptr;
  MapBoundaryFogRenderer* m_boundary_fog = nullptr;
  AmbientFogRenderer* m_ambient_fog = nullptr;
  std::vector<IRenderPass*> m_passes;

  mutable Render::WorldView m_world_view;
};

} // namespace Render::GL
