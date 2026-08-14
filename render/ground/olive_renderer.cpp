#include "olive_renderer.h"

#include <QVector2D>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "decoration_gpu.h"
#include "game/map/scatter/ground_utils.h"
#include "game/map/scatter/scatter_composition.h"
#include "game/map/scatter/spawn_validator.h"
#include "gl/render_constants.h"
#include "gl/resources.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "render/scene_renderer.h"
#include "scatter_runtime.h"

namespace {

using std::uint32_t;
using namespace Render::Ground;

constexpr int k_tree_cell_span = 4;
constexpr float k_tree_density_area_scale = 16.0F / 36.0F;
constexpr float k_tree_edge_padding_scale = 0.35F;

auto resolve_tree_surface_position(const Game::Map::TerrainService& terrain_service,
                                   float world_x,
                                   float world_z,
                                   float fallback_y) -> QVector3D {
  if (terrain_service.is_initialized()) {
    return terrain_service.resolve_surface_world_position(
        world_x, world_z, 0.0F, fallback_y);
  }
  return {world_x, fallback_y, world_z};
}

} // namespace

namespace Render::GL {

OliveRenderer::OliveRenderer() = default;
OliveRenderer::~OliveRenderer() = default;

void OliveRenderer::configure(
    const Game::Map::TerrainHeightMap& height_map,
    const Game::Map::BiomeSettings& biome_settings,
    const std::vector<Game::Map::WorldProp>& scatter_seed_world_props,
    const std::vector<Game::Map::WorldProp>& runtime_world_props,
    bool use_world_props_exclusively) {
  configure_height_scatter_common(height_map,
                                  biome_settings,
                                  scatter_seed_world_props,
                                  runtime_world_props,
                                  use_world_props_exclusively);

  const auto wind_profile = Game::Map::make_wind_profile(m_biome_settings);
  auto& olive_params = m_state.params;
  olive_params.light_direction = m_light_direction;
  olive_params.time = 0.0F;
  olive_params.wind_strength = wind_profile.sway_strength;
  olive_params.wind_speed = wind_profile.sway_speed;

  rebuild_olive_instances();
}

void OliveRenderer::refresh_world_props(
    const std::vector<Game::Map::WorldProp>& runtime_world_props,
    bool use_world_props_exclusively) {
  adopt_runtime_world_props(runtime_world_props, use_world_props_exclusively);
  rebuild_olive_instances();
}

void OliveRenderer::rebuild_olive_instances() {
  m_state.instances.clear();
  append_world_prop_olives();
  if (!m_use_world_props_exclusively) {
    append_procedural_instances(
        [this](std::vector<TreeInstanceGpu>& out) { generate_procedural_olives(out); });
  }
  finish_instance_rebuild();
}

void OliveRenderer::set_light_direction(const QVector3D& dir) {
  set_light_direction_common(dir, QVector3D(0.35F, 0.8F, 0.45F));
}

void OliveRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  submit_filtered_common<true>(
      renderer,
      resources,
      TerrainScatterCmd::Species::Olive,
      [](TerrainScatterCmd& cmd, const FoliageBatchParams& params) {
        cmd.foliage = params;
      });
}

void OliveRenderer::append_world_prop_olives() {
  auto& olive_instances = m_state.instances;

  {
    const auto& terrain_service = world().terrain_or_empty();
    for (const auto& prop : m_runtime_world_props) {
      if (prop.type != Game::Map::WorldProp::Type::OliveTree) {
        continue;
      }
      const QVector3D pos = terrain_service.world_prop_world_position(prop);

      uint32_t var_state = hash_coords(static_cast<int>(std::round(prop.x)),
                                       static_cast<int>(std::round(prop.z)),
                                       m_noise_seed ^ 0x7B3E5F1CU);
      const float color_var = rand_01(var_state);
      const QVector3D base_color(0.215F, 0.290F, 0.185F);
      const QVector3D var_color(0.480F, 0.570F, 0.340F);
      QVector3D tint = base_color * (1.0F - color_var) + var_color * color_var;

      tint *= remap(rand_01(var_state), 0.82F, 1.20F);
      const float sway_phase = rand_01(var_state) * MathConstants::k_two_pi;
      const float silhouette_seed = rand_01(var_state);
      const float leaf_seed = rand_01(var_state);
      const float bark_seed = rand_01(var_state);

      TreeInstanceGpu inst;
      inst.pos_scale =
          QVector4D(pos.x(),
                    pos.y(),
                    pos.z(),
                    prop.scale * Game::Map::world_prop_render_scale(
                                     Game::Map::WorldProp::Type::OliveTree));
      inst.color_sway = QVector4D(tint.x(), tint.y(), tint.z(), sway_phase);
      inst.rotation = QVector4D(prop.rotation, silhouette_seed, leaf_seed, bark_seed);
      olive_instances.push_back(inst);
    }
  }
}

void OliveRenderer::generate_procedural_olives(
    std::vector<TreeInstanceGpu>& out) const {
  auto& olive_instances = out;

  if (m_width < 2 || m_height < 2 || m_height_data.empty()) {
    return;
  }

  const auto scatter_profile = Game::Map::make_scatter_profile(m_biome_settings);
  const auto scatter_rules = Game::Map::make_scatter_rules(scatter_profile.ground_type);
  if (!scatter_rules.allow_olives) {
    return;
  }

  const float tile_safe = std::max(0.1F, m_tile_size);

  float olive_density = scatter_rules.olive_base_density;
  if (scatter_profile.plant_density > 0.0F) {
    olive_density = scatter_profile.plant_density * scatter_rules.olive_density_scale;
  }

  SpawnTerrainCache terrain_cache;
  terrain_cache.build_from_height_map(
      m_height_data, m_terrain_types, m_width, m_height, m_tile_size);

  SpawnValidationConfig config = make_tree_spawn_config();
  config.grid_width = m_width;
  config.grid_height = m_height;
  config.tile_size = m_tile_size;
  config.edge_padding = scatter_profile.spawn_edge_padding * k_tree_edge_padding_scale;
  config.max_slope = 0.65F;

  SpawnValidator validator(terrain_cache, config);
  ScatterCompositionContext composition(terrain_cache,
                                        m_width,
                                        m_height,
                                        m_tile_size,
                                        m_biome_settings,
                                        m_scatter_seed_world_props);

  auto add_olive = [&](float gx, float gz, uint32_t& state) -> bool {
    if (!validator.can_spawn_at_grid(gx, gz)) {
      return false;
    }

    auto const scene = composition.sample_grid(gx, gz, state ^ 0x16C92A4FU);
    if (scene.obstacle_influence >= 1.0F) {
      return false;
    }
    if (rand_01(state) > scatter_spawn_chance(ScatterRuleSpecies::Olive, scene)) {
      return false;
    }

    float world_x = 0.0F;
    float world_z = 0.0F;
    validator.grid_to_world(gx, gz, world_x, world_z);
    QVector3D const world_pos =
        resolve_tree_surface_position(world().terrain_or_empty(),
                                      world_x,
                                      world_z,
                                      terrain_cache.sample_height_at(gx, gz));

    float const color_var = remap(rand_01(state), 0.0F, 1.0F);

    QVector3D const base_color(0.160F + scene.dryness * 0.05F,
                               0.245F + scene.rockiness * 0.04F,
                               0.170F + scene.dryness * 0.03F);
    QVector3D const var_color(0.495F + scene.cluster_bias * 0.05F,
                              0.605F + scene.rockiness * 0.05F,
                              0.355F + scene.cluster_bias * 0.04F);
    QVector3D tint_color = base_color * (1.0F - color_var) + var_color * color_var;

    float const gray_mix = remap(
        rand_01(state), 0.08F + scene.rockiness * 0.04F, 0.18F + scene.dryness * 0.08F);
    QVector3D const gray_tint(0.530F, 0.560F, 0.500F);
    tint_color = tint_color * (1.0F - gray_mix) + gray_tint * gray_mix;

    tint_color *= remap(rand_01(state), 0.80F, 1.22F);

    float const sway_phase = rand_01(state) * MathConstants::k_two_pi;

    float const rotation = rand_01(state) * MathConstants::k_two_pi;

    float const silhouette_seed = rand_01(state);
    float const leaf_seed = rand_01(state);
    float const bark_seed = rand_01(state);

    TreeInstanceGpu instance;

    float const chosen_scale = remap(rand_01(state),
                                     scatter_rules.olive_scale_min,
                                     scatter_rules.olive_scale_max) *
                               tile_safe *
                               scatter_scale_bias(ScatterRuleSpecies::Olive, scene);

    instance.pos_scale =
        QVector4D(world_pos.x(), world_pos.y(), world_pos.z(), chosen_scale);
    instance.color_sway =
        QVector4D(tint_color.x(), tint_color.y(), tint_color.z(), sway_phase);
    instance.rotation = QVector4D(rotation, silhouette_seed, leaf_seed, bark_seed);
    olive_instances.push_back(instance);
    return true;
  };

  for (int z = 0; z < m_height; z += k_tree_cell_span) {
    for (int x = 0; x < m_width; x += k_tree_cell_span) {
      int const sample_x = std::min(x + k_tree_cell_span / 2, m_width - 1);
      int const sample_z = std::min(z + k_tree_cell_span / 2, m_height - 1);
      int const idx = sample_z * m_width + sample_x;

      float const slope = terrain_cache.get_slope_at(sample_x, sample_z);
      if (slope > 0.65F) {
        continue;
      }

      uint32_t state =
          hash_coords(x, z, m_noise_seed ^ 0xCD34EF56U ^ static_cast<uint32_t>(idx));
      auto const cell_scene = composition.sample_grid(static_cast<float>(sample_x),
                                                      static_cast<float>(sample_z),
                                                      state ^ 0x62D1E7AFU);

      Game::Map::TerrainType const terrain_type =
          terrain_cache.get_terrain_type_at(sample_x, sample_z);
      float density_mult = 1.0F;
      if (terrain_type == Game::Map::TerrainType::Hill) {
        density_mult = 1.15F;
      } else if (terrain_type == Game::Map::TerrainType::Mountain) {
        density_mult = 0.5F;
      } else if (terrain_type == Game::Map::TerrainType::Forest) {
        density_mult = 0.45F;
      }

      uint32_t cls_state = hash_coords(x / 8, z / 8, m_noise_seed ^ 0xC7E4F1A3U);
      float const macro_noise = rand_01(cls_state);
      uint32_t mid_state = hash_coords(x / 4, z / 4, m_noise_seed ^ 0xA2B5D8E6U);
      float const mid_noise = rand_01(mid_state);
      float const cluster_noise = macro_noise * 0.65F + mid_noise * 0.35F;
      float const cluster_mult = 0.40F + cluster_noise * cluster_noise * 1.65F;
      density_mult *= scatter_density_multiplier(ScatterRuleSpecies::Olive, cell_scene);

      float const effective_density = olive_density * density_mult *
                                      (0.70F + cell_scene.cluster_bias * 1.05F) *
                                      k_tree_density_area_scale * cluster_mult;
      if (effective_density < 0.04F) {
        continue;
      }
      if (rand_01(state) >
          scatter_spawn_chance(ScatterRuleSpecies::Olive, cell_scene)) {
        continue;
      }
      int olive_count = static_cast<int>(std::floor(effective_density));
      float const frac = effective_density - float(olive_count);
      if (rand_01(state) < frac) {
        olive_count += 1;
      }

      for (int i = 0; i < olive_count; ++i) {
        float const gx = float(x) + rand_01(state) * float(k_tree_cell_span);
        float const gz = float(z) + rand_01(state) * float(k_tree_cell_span);
        if (!add_olive(gx, gz, state)) {
          continue;
        }

        auto const leader_scene = composition.sample_grid(gx, gz, state ^ 0x0E63A1B2U);
        int const satellite_count = scatter_cluster_satellite_count(
            ScatterRuleSpecies::Olive, leader_scene, state);
        for (int satellite = 0; satellite < satellite_count; ++satellite) {
          float const angle = rand_01(state) * MathConstants::k_two_pi;
          float const radius_tiles = scatter_cluster_radius_tiles(
              ScatterRuleSpecies::Olive, leader_scene, state);
          add_olive(gx + std::cos(angle) * radius_tiles,
                    gz + std::sin(angle) * radius_tiles,
                    state);
        }
      }
    }
  }
}

} // namespace Render::GL
