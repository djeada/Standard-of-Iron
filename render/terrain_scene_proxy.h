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
#include "ground/terrain_feature_manager.h"
#include "ground/terrain_renderer.h"
#include "ground/terrain_scatter_manager.h"
#include "ground/terrain_surface_manager.h"
#include "terrain_scene_types.h"
#include <vector>

namespace Render::GL {

class Renderer;
class ResourceManager;

class TerrainSceneProxy {
public:
  TerrainSceneProxy(TerrainSurfaceManager *surface,
                    TerrainFeatureManager *features,
                    TerrainScatterManager *scatter, RainRenderer *rain,
                    FogRenderer *fog)
      : m_surface(surface), m_features(features), m_scatter(scatter),
        m_rain(rain), m_fog(fog) {
    if (m_surface != nullptr) {
      const auto &surface_passes = m_surface->passes();
      m_passes.insert(m_passes.end(), surface_passes.begin(),
                      surface_passes.end());
    }
    if (m_features != nullptr) {
      const auto &feature_passes = m_features->passes();
      m_passes.insert(m_passes.end(), feature_passes.begin(),
                      feature_passes.end());
    }
    if (m_scatter != nullptr) {
      const auto &scatter_passes = m_scatter->passes();
      m_passes.insert(m_passes.end(), scatter_passes.begin(),
                      scatter_passes.end());
    }
    m_passes.push_back(rain);
    m_passes.push_back(fog);
  }

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

  [[nodiscard]] auto surface() const -> TerrainSurfaceManager * {
    return m_surface;
  }
  [[nodiscard]] auto ground() const -> GroundRenderer * {
    return m_surface != nullptr ? m_surface->ground() : nullptr;
  }
  [[nodiscard]] auto terrain() const -> TerrainRenderer * {
    return m_surface != nullptr ? m_surface->terrain() : nullptr;
  }
  [[nodiscard]] auto river() const -> RiverRenderer * {
    return m_features != nullptr ? m_features->river() : nullptr;
  }
  [[nodiscard]] auto road() const -> RoadRenderer * {
    return m_features != nullptr ? m_features->road() : nullptr;
  }
  [[nodiscard]] auto riverbank() const -> RiverbankRenderer * {
    return m_features != nullptr ? m_features->riverbank() : nullptr;
  }
  [[nodiscard]] auto bridge() const -> BridgeRenderer * {
    return m_features != nullptr ? m_features->bridge() : nullptr;
  }
  [[nodiscard]] auto biome() const -> BiomeRenderer * {
    return m_scatter != nullptr ? m_scatter->biome() : nullptr;
  }
  [[nodiscard]] auto stone() const -> StoneRenderer * {
    return m_scatter != nullptr ? m_scatter->stone() : nullptr;
  }
  [[nodiscard]] auto plant() const -> PlantRenderer * {
    return m_scatter != nullptr ? m_scatter->plant() : nullptr;
  }
  [[nodiscard]] auto pine() const -> PineRenderer * {
    return m_scatter != nullptr ? m_scatter->pine() : nullptr;
  }
  [[nodiscard]] auto olive() const -> OliveRenderer * {
    return m_scatter != nullptr ? m_scatter->olive() : nullptr;
  }
  [[nodiscard]] auto firecamp() const -> FireCampRenderer * {
    return m_scatter != nullptr ? m_scatter->firecamp() : nullptr;
  }
  [[nodiscard]] auto rain() const -> RainRenderer * { return m_rain; }
  [[nodiscard]] auto fog() const -> FogRenderer * { return m_fog; }

  [[nodiscard]] auto has_field() const -> bool {
    return Game::Map::TerrainService::instance().is_initialized();
  }

  [[nodiscard]] auto field() const -> const Game::Map::TerrainField & {
    return Game::Map::TerrainService::instance().terrain_field();
  }

  [[nodiscard]] auto
  road_segments() const -> const std::vector<Game::Map::RoadSegment> & {
    return Game::Map::TerrainService::instance().road_segments();
  }

  [[nodiscard]] auto
  surface_chunks() const -> std::vector<TerrainSurfaceChunk> {
    auto &terrain_service = Game::Map::TerrainService::instance();
    TerrainSurfaceShaderParams base_params;
    if (terrain_service.is_initialized()) {
      base_params.biome_settings = &terrain_service.biome_settings();
      base_params.field = &terrain_service.terrain_field();
    }

    return {{TerrainSurfaceKind::GroundPlane, ground(),
             TerrainSurfaceShaderParams{base_params.biome_settings,
                                        base_params.field, true}},
            {TerrainSurfaceKind::TerrainMesh, terrain(),
             TerrainSurfaceShaderParams{base_params.biome_settings,
                                        base_params.field, false}}};
  }

  [[nodiscard]] auto feature_chunks() const -> std::vector<LinearFeatureChunk> {
    auto &terrain = Game::Map::TerrainService::instance();
    auto const *height_map = terrain.get_height_map();
    size_t const river_count =
        (height_map != nullptr) ? height_map->get_river_segments().size() : 0U;
    size_t const bridge_count =
        (height_map != nullptr) ? height_map->get_bridges().size() : 0U;
    static const std::vector<IRenderPass *> k_empty_feature_passes;
    auto const &feature_passes =
        (m_features != nullptr) ? m_features->passes() : k_empty_feature_passes;
    auto pass_at = [&](std::size_t index) -> IRenderPass * {
      return index < feature_passes.size() ? feature_passes[index] : nullptr;
    };

    return std::vector<LinearFeatureChunk>{
        LinearFeatureChunk{LinearFeatureKind::River,
                           LinearFeatureVisibilityMode::SegmentSampled,
                           pass_at(0), river_count},
        LinearFeatureChunk{LinearFeatureKind::Road,
                           LinearFeatureVisibilityMode::SegmentSampled,
                           pass_at(1), road_segments().size()},
        LinearFeatureChunk{LinearFeatureKind::Riverbank,
                           LinearFeatureVisibilityMode::TextureDriven,
                           pass_at(2), river_count},
        LinearFeatureChunk{LinearFeatureKind::Bridge,
                           LinearFeatureVisibilityMode::SegmentSampled,
                           pass_at(3), bridge_count}};
  }

  [[nodiscard]] auto scatter_chunks() const -> std::vector<ScatterChunk> {
    auto *grass = biome();
    auto *stone_renderer = stone();
    auto *plant_renderer = plant();
    auto *pine_renderer = pine();
    auto *olive_renderer = olive();
    auto *firecamp_renderer = firecamp();

    return {
        {ScatterSpeciesId::Grass, ScatterVisibilityMode::None, grass,
         grass != nullptr ? grass->instance_count() : 0U,
         grass == nullptr || grass->is_gpu_ready()},
        {ScatterSpeciesId::Stone, ScatterVisibilityMode::None, stone_renderer,
         stone_renderer != nullptr ? stone_renderer->instance_count() : 0U,
         stone_renderer == nullptr || stone_renderer->is_gpu_ready()},
        {ScatterSpeciesId::Plant, ScatterVisibilityMode::InstanceFiltered,
         plant_renderer,
         plant_renderer != nullptr ? plant_renderer->instance_count() : 0U,
         plant_renderer == nullptr || plant_renderer->is_gpu_ready()},
        {ScatterSpeciesId::Pine, ScatterVisibilityMode::InstanceFiltered,
         pine_renderer,
         pine_renderer != nullptr ? pine_renderer->instance_count() : 0U,
         pine_renderer == nullptr || pine_renderer->is_gpu_ready()},
        {ScatterSpeciesId::Olive, ScatterVisibilityMode::InstanceFiltered,
         olive_renderer,
         olive_renderer != nullptr ? olive_renderer->instance_count() : 0U,
         olive_renderer == nullptr || olive_renderer->is_gpu_ready()},
        {ScatterSpeciesId::FireCamp, ScatterVisibilityMode::InstanceFiltered,
         firecamp_renderer,
         firecamp_renderer != nullptr ? firecamp_renderer->instance_count()
                                      : 0U,
         firecamp_renderer == nullptr || firecamp_renderer->is_gpu_ready()}};
  }

  [[nodiscard]] auto passes() const -> const std::vector<IRenderPass *> & {
    return m_passes;
  }

private:
  void submit_surfaces(Renderer &renderer, ResourceManager *resources) const {
    if (m_surface != nullptr) {
      m_surface->submit(renderer, resources);
    }
  }

  void submit_features(Renderer &renderer, ResourceManager *resources) const {
    if (m_features != nullptr) {
      m_features->submit(renderer, resources);
    }
  }

  void submit_scatters(Renderer &renderer, ResourceManager *resources) const {
    if (m_scatter != nullptr) {
      m_scatter->submit(renderer, resources);
    }
  }

  TerrainSurfaceManager *m_surface = nullptr;
  TerrainFeatureManager *m_features = nullptr;
  TerrainScatterManager *m_scatter = nullptr;
  RainRenderer *m_rain = nullptr;
  FogRenderer *m_fog = nullptr;
  std::vector<IRenderPass *> m_passes;
};

} // namespace Render::GL
