#pragma once

#include <QVector3D>
#include <QVector4D>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "../../game/map/map_definition.h"
#include "../../game/map/terrain.h"
#include "../decoration_gpu.h"
#include "../scene_renderer.h"
#include "i_scatter_pass.h"
#include "scatter_renderer_state.h"
#include "scatter_submission.h"

namespace Render::GL {

template <typename Instance, typename Params>
class ScatterRendererBase : public IScatterPass {
public:
  [[nodiscard]] auto is_gpu_ready() const -> bool override {
    return m_state.is_gpu_ready();
  }

  [[nodiscard]] auto instance_count() const -> std::size_t override {
    return m_state.instances.size();
  }

  [[nodiscard]] auto
  last_sync_stats() const -> Render::Ground::Scatter::SyncStats override {
    return m_state.last_sync_stats;
  }

  void clear() override { m_state.reset_instances(); }

protected:
  using State = Render::Ground::Scatter::FilteredRendererState<Instance, Params>;

  void configure_height_scatter_common(
      const Game::Map::TerrainHeightMap& height_map,
      const Game::Map::BiomeSettings& biome_settings,
      const std::vector<Game::Map::WorldProp>& scatter_seed_world_props,
      const std::vector<Game::Map::WorldProp>& runtime_world_props,
      bool use_world_props_exclusively) {
    m_width = height_map.get_width();
    m_height = height_map.get_height();
    m_tile_size = height_map.get_tile_size();
    m_height_data = height_map.get_height_data();
    m_terrain_types = height_map.getTerrainTypes();
    m_scatter_seed_world_props = scatter_seed_world_props;
    m_runtime_world_props = runtime_world_props;
    m_world_props = runtime_world_props;
    m_use_world_props_exclusively = use_world_props_exclusively;
    m_biome_settings = biome_settings;
    m_noise_seed = biome_settings.seed;
    m_state.reset_instances();
    invalidate_procedural_cache();
  }

  void configure_biome_common(const Game::Map::BiomeSettings& biome_settings,
                              bool use_world_props_exclusively = false) {
    m_biome_settings = biome_settings;
    m_use_world_props_exclusively = use_world_props_exclusively;
    m_state.reset_instances();
    invalidate_procedural_cache();
  }

  void adopt_runtime_world_props(
      const std::vector<Game::Map::WorldProp>& runtime_world_props,
      bool use_world_props_exclusively) {
    m_runtime_world_props = runtime_world_props;
    m_world_props = runtime_world_props;
    m_use_world_props_exclusively = use_world_props_exclusively;
  }

  void invalidate_procedural_cache() {
    m_procedural_cached = false;
    m_procedural_instances.clear();
    m_procedural_generations = 0;
  }

  template <typename GenerateProcedural>
  void append_procedural_instances(GenerateProcedural&& generate) {
    if (!m_procedural_cached) {
      m_procedural_instances.clear();
      generate(m_procedural_instances);
      m_procedural_cached = true;
      ++m_procedural_generations;
    }
    m_state.instances.insert(m_state.instances.end(),
                             m_procedural_instances.begin(),
                             m_procedural_instances.end());
  }

  void finish_instance_rebuild() {
    m_state.instance_count = m_state.instances.size();
    m_state.instances_dirty = m_state.instance_count > 0;
  }

public:
  [[nodiscard]] auto procedural_generations_for_test() const -> std::size_t {
    return m_procedural_generations;
  }

protected:
  void set_light_direction_common(const QVector3D& dir,
                                  const QVector3D& default_direction) {
    m_light_direction = dir.isNull() ? default_direction : dir.normalized();
    m_state.params.light_direction = m_light_direction;
  }

  template <bool CopyTimeToParams>
  auto submit_filtered_common(Renderer& renderer,
                              ResourceManager* resources,
                              TerrainScatterCmd::Species species,
                              void (*assign_params)(TerrainScatterCmd&,
                                                    const Params&)) -> std::size_t {
    (void)resources;

    const auto visible_count = Render::Ground::Scatter::sync_filtered_state(
        m_state,
        [](const Instance& instance) -> const QVector4D& { return instance.pos_scale; },
        renderer.static_world_visibility_filter_enabled()
            ? renderer.submission_visibility().snapshot()
            : nullptr,

        Render::Ground::Scatter::ScatterMemoryMode::Remembered);
    if (visible_count == 0) {
      return 0;
    }

    TerrainScatterCmd cmd;
    cmd.species = species;
    cmd.visibility = renderer.visibility_mask();
    if constexpr (CopyTimeToParams) {
      Params params = m_state.params;
      params.time = renderer.get_animation_time();
      assign_params(cmd, params);
    } else {
      assign_params(cmd, m_state.params);
    }
    Render::Ground::Scatter::submit_visible_chunks(renderer, m_state, cmd);
    return visible_count;
  }

  auto submit_prop_common(Renderer& renderer,
                          ResourceManager* resources,
                          TerrainScatterCmd::Species species) -> std::size_t {
    m_state.params.time = renderer.get_animation_time();
    return submit_filtered_common<false>(
        renderer, resources, species, [](TerrainScatterCmd& cmd, const Params& params) {
          cmd.prop = params;
        });
  }

  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;

  std::vector<float> m_height_data;
  std::vector<Game::Map::TerrainType> m_terrain_types;
  std::vector<Game::Map::WorldProp> m_scatter_seed_world_props;
  std::vector<Game::Map::WorldProp> m_runtime_world_props;
  std::vector<Game::Map::WorldProp> m_world_props;
  bool m_use_world_props_exclusively = false;
  Game::Map::BiomeSettings m_biome_settings;
  std::uint32_t m_noise_seed = 0U;
  QVector3D m_light_direction{0.35F, 0.8F, 0.45F};

  std::vector<Instance> m_procedural_instances;
  bool m_procedural_cached = false;
  std::size_t m_procedural_generations = 0;

  State m_state;
};

} // namespace Render::GL
