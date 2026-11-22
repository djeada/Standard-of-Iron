#pragma once

#include <QVector3D>
#include <QVector4D>

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "game/map/biome_settings.h"
#include "game/map/map_definition.h"
#include "game/map/terrain.h"
#include "render/frame/decoration_gpu.h"
#include "render/frame/frame_submitter.h"
#include "render/ground/i_scatter_source.h"
#include "render/ground/scatter/scatter_request.h"
#include "render/ground/scatter_renderer_state.h"
#include "render/ground/scatter_submission.h"

namespace Render::GL {

template <typename Instance, typename Params>
class ScatterRendererBase : public IScatterSource {
public:
  [[nodiscard]] auto is_gpu_ready() const -> bool override {
    return m_state.is_gpu_ready();
  }

  [[nodiscard]] auto instance_count() const -> std::size_t override {
    return m_state.instances.size();
  }

  [[nodiscard]] auto instance_bytes() const -> std::size_t override {
    return m_state.instances.size() * sizeof(Instance);
  }

  [[nodiscard]] auto instance_stride() const -> std::size_t override {
    return sizeof(Instance);
  }

  [[nodiscard]] auto
  last_sync_stats() const -> Render::Ground::Scatter::SyncStats override {
    return m_state.last_sync_stats;
  }

  void clear() override { m_state.reset_instances(); }

  void set_instance_budget(std::size_t budget) override { m_instance_budget = budget; }
  [[nodiscard]] auto instance_budget() const -> std::size_t override {
    return m_instance_budget;
  }
  [[nodiscard]] auto budget_hit() const -> bool override { return m_budget_hit; }

  [[nodiscard]] static auto
  instance_position(const Instance& instance) -> const QVector4D& {
    if constexpr (requires { instance.pos_scale; }) {
      return instance.pos_scale;
    } else if constexpr (requires { instance.pos_intensity; }) {
      return instance.pos_intensity;
    } else {
      return instance.pos_height;
    }
  }

  void prepare_instance_blocks() override {
    Render::Ground::Scatter::prepare_filtered_blocks(m_state, &instance_position);
  }

  void for_each_instance_block(const InstanceBlockVisitor& visit) const override {
    Render::Ground::Scatter::for_each_filtered_block(m_state, visit);
  }

protected:
  using State = Render::Ground::Scatter::FilteredRendererState<Instance, Params>;

  void configure_height_scatter_common(
      const Game::Map::TerrainHeightMap& height_map,
      const Game::Map::BiomeSettings& biome_settings,
      const std::vector<Game::Map::WorldProp>& world_props) {
    m_width = height_map.get_width();
    m_height = height_map.get_height();
    m_tile_size = height_map.get_tile_size();
    m_height_data = height_map.get_height_data();
    m_terrain_types = height_map.getTerrainTypes();
    m_world_props = world_props;
    m_biome_settings = biome_settings;
    m_noise_seed = biome_settings.seed;
    m_state.reset_instances();
  }

  void configure_biome_common(const Game::Map::BiomeSettings& biome_settings) {
    m_biome_settings = biome_settings;
    m_state.reset_instances();
  }

  // The terrain grid copied at configure, for a generator request.
  [[nodiscard]] auto scatter_grid() const -> Render::Ground::Scatter::ScatterGrid {
    return {m_width, m_height, m_tile_size, &m_height_data, &m_terrain_types};
  }

  // Takes a generator's output as the layer's instances, cut to the budget.
  void adopt_generated(std::vector<Instance> instances) {
    m_budget_hit =
        Render::Ground::Scatter::apply_instance_budget(instances, m_instance_budget);
    m_state.instances = std::move(instances);
    finish_instance_rebuild();
  }

  void finish_instance_rebuild() {
    m_state.instance_count = m_state.instances.size();
    m_state.instances_dirty = m_state.instance_count > 0;
  }

public:
  [[nodiscard]] virtual auto fog_culls_instances() const -> bool { return true; }

protected:
  void set_light_direction_common(const QVector3D& dir,
                                  const QVector3D& default_direction) {
    m_light_direction = dir.isNull() ? default_direction : dir.normalized();
    m_state.params.light_direction = m_light_direction;
  }

  template <bool CopyTimeToParams>
  auto submit_filtered_common(FrameSubmitter& renderer,
                              const DrawResources* resources,
                              TerrainScatterCmd::Species species,
                              void (*assign_params)(TerrainScatterCmd&,
                                                    const Params&)) -> std::size_t {
    (void)resources;

    const auto visible_count = Render::Ground::Scatter::sync_filtered_state(
        m_state,
        &instance_position,
        (fog_culls_instances() && renderer.static_world_visibility_filter_enabled())
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

  auto submit_prop_common(FrameSubmitter& renderer,
                          const DrawResources* resources,
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
  std::vector<Game::Map::WorldProp> m_world_props;
  Game::Map::BiomeSettings m_biome_settings;
  std::uint32_t m_noise_seed = 0U;
  QVector3D m_light_direction{0.35F, 0.8F, 0.45F};

  State m_state;
  std::size_t m_instance_budget = Render::Ground::Scatter::k_unbounded_instances;
  bool m_budget_hit = false;
};

} // namespace Render::GL
