#include "boulder_renderer.h"

#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "../scene_renderer.h"
#include "decoration_gpu.h"
#include "game/map/scatter/ground_utils.h"
#include "game/map/scatter/scatter_composition.h"
#include "game/map/scatter/spawn_validator.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "scatter_runtime.h"

namespace {

using std::uint32_t;
using namespace Render::Ground;
constexpr float k_reference_scatter_extent = 220.0F;

} // namespace

namespace Render::GL {

BoulderRenderer::BoulderRenderer() = default;
BoulderRenderer::~BoulderRenderer() = default;

void BoulderRenderer::configure(
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
  m_state.params.light_direction = m_light_direction;
  rebuild_boulder_instances();
}

void BoulderRenderer::refresh_world_props(
    const std::vector<Game::Map::WorldProp>& runtime_world_props,
    bool use_world_props_exclusively) {
  adopt_runtime_world_props(runtime_world_props, use_world_props_exclusively);
  rebuild_boulder_instances();
}

void BoulderRenderer::rebuild_boulder_instances() {
  m_state.instances.clear();
  append_world_prop_boulders();
  if (!m_use_world_props_exclusively) {
    append_procedural_instances([this](std::vector<StoneInstanceGpu>& out) {
      generate_procedural_boulders(out);
    });
  }
  finish_instance_rebuild();
}

void BoulderRenderer::set_light_direction(const QVector3D& dir) {
  set_light_direction_common(dir, StoneBatchParams::default_light_direction());
}

void BoulderRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  submit_filtered_common<false>(
      renderer,
      resources,
      TerrainScatterCmd::Species::Stone,
      [](TerrainScatterCmd& cmd, const StoneBatchParams& params) {
        cmd.stone = params;
      });
}

void BoulderRenderer::append_world_prop_boulders() {
  auto& terrain_service = Game::Map::TerrainService::instance();
  const auto surface_profile = Game::Map::make_surface_profile(m_biome_settings);

  for (const auto& prop : m_runtime_world_props) {
    if (prop.type != Game::Map::WorldProp::Type::Boulder) {
      continue;
    }
    const QVector3D resolved = terrain_service.world_prop_world_position(prop);

    uint32_t state = hash_coords(static_cast<int>(prop.x),
                                 static_cast<int>(prop.z),
                                 m_biome_settings.seed ^ 0xD3A4B1C2U);
    float const color_var = remap(rand_01(state), 0.0F, 1.0F);
    QVector3D const base_rock = surface_profile.rock_low;
    QVector3D const high_rock = surface_profile.rock_high;
    QVector3D color = base_rock * (1.0F - color_var) + high_rock * color_var;
    float const earth_mix = remap(rand_01(state), 0.08F, 0.30F);
    QVector3D const earth_tint(0.34F, 0.31F, 0.27F);
    color = color * (1.0F - earth_mix) + earth_tint * earth_mix;
    color *= 0.86F;

    StoneInstanceGpu inst;
    inst.pos_scale = QVector4D(resolved.x(),
                               resolved.y(),
                               resolved.z(),
                               prop.scale * Game::Map::world_prop_render_scale(
                                                Game::Map::WorldProp::Type::Boulder));
    inst.color_rot = QVector4D(color.x(), color.y(), color.z(), prop.rotation);
    m_state.instances.push_back(inst);
  }
}

void BoulderRenderer::generate_procedural_boulders(
    std::vector<StoneInstanceGpu>& out) const {
  if (m_width < 2 || m_height < 2 || m_height_data.empty()) {
    return;
  }

  const auto scatter_profile = Game::Map::make_scatter_profile(m_biome_settings);
  const auto surface_profile = Game::Map::make_surface_profile(m_biome_settings);

  SpawnTerrainCache terrain_cache;
  terrain_cache.build_from_height_map(
      m_height_data, m_terrain_types, m_width, m_height, m_tile_size);

  SpawnValidationConfig config = make_stone_spawn_config();
  config.grid_width = m_width;
  config.grid_height = m_height;
  config.tile_size = m_tile_size;
  config.edge_padding = scatter_profile.spawn_edge_padding * 0.45F;
  config.max_slope = 0.38F;
  config.allow_hill = true;
  config.road_clearance = 1.8F;
  config.river_clearance = 1.4F;

  SpawnValidator validator(terrain_cache, config);
  ScatterCompositionContext composition(terrain_cache,
                                        m_width,
                                        m_height,
                                        m_tile_size,
                                        m_biome_settings,
                                        m_scatter_seed_world_props);

  auto add_boulder = [&](float gx,
                         float gz,
                         float scale_min,
                         float scale_max,
                         uint32_t& state) -> bool {
    if (!validator.can_spawn_at_grid(gx, gz)) {
      return false;
    }

    auto const scene = composition.sample_grid(gx, gz, state ^ 0xE148D9A7U);
    float const support = std::max(
        scene.rockiness, std::max(scene.prop_influence * 0.90F, scene.dryness * 0.42F));
    if (support < 0.16F && scene.archetype != ScatterSceneArchetype::RockyPatch &&
        scene.archetype != ScatterSceneArchetype::DryClearing &&
        scene.prop_influence < 0.28F) {
      return false;
    }
    (void)rand_01(state);

    float world_x = 0.0F;
    float world_z = 0.0F;
    validator.grid_to_world(gx, gz, world_x, world_z);
    float const world_y = terrain_cache.sample_height_at(gx, gz);

    float const color_var = remap(rand_01(state), 0.0F, 1.0F);
    QVector3D color = surface_profile.rock_low * (1.0F - color_var) +
                      surface_profile.rock_high * color_var;
    QVector3D const earth_tint(0.34F, 0.31F, 0.27F);
    float const earth_mix = remap(rand_01(state), 0.10F, 0.25F + scene.dryness * 0.10F);
    color = color * (1.0F - earth_mix) + earth_tint * earth_mix;
    color *= 0.82F + scene.rockiness * 0.08F;

    StoneInstanceGpu inst;
    float const scale = remap(rand_01(state), scale_min, scale_max) *
                        scatter_scale_bias(ScatterRuleSpecies::Stone, scene);
    inst.pos_scale = QVector4D(world_x, world_y + 0.01F, world_z, scale);
    inst.color_rot = QVector4D(
        color.x(), color.y(), color.z(), rand_01(state) * MathConstants::k_two_pi);
    out.push_back(inst);
    return true;
  };

  {
    float const base_density =
        std::clamp(0.090F + m_biome_settings.rock_exposure * 0.120F -
                       m_biome_settings.moisture_level * 0.020F,
                   0.055F,
                   0.200F);
    float const area_scale =
        std::sqrt(static_cast<float>(std::max(m_width, 1) * std::max(m_height, 1)) /
                  (k_reference_scatter_extent * k_reference_scatter_extent));
    int const sampling_scale = std::max(1, static_cast<int>(std::round(area_scale)));
    int const cell_span = 6 * sampling_scale;
    for (int z = 0; z < m_height; z += cell_span) {
      for (int x = 0; x < m_width; x += cell_span) {
        int const sample_x = std::min(x + cell_span / 2, m_width - 1);
        int const sample_z = std::min(z + cell_span / 2, m_height - 1);
        uint32_t state = hash_coords(x, z, m_biome_settings.seed ^ 0x6D1A75B3U);
        auto const scene = composition.sample_grid(static_cast<float>(sample_x),
                                                   static_cast<float>(sample_z),
                                                   state ^ 0xA28C52EFU);
        float const density =
            base_density *
            scatter_density_multiplier(ScatterRuleSpecies::Stone, scene) *
            (0.72F + scene.cluster_bias * 1.40F);
        if (rand_01(state) > density) {
          continue;
        }
        float const gx = static_cast<float>(x) + rand_01(state) * float(cell_span);
        float const gz = static_cast<float>(z) + rand_01(state) * float(cell_span);
        add_boulder(gx, gz, 1.15F, 2.45F, state);
      }
    }
  }
}

} // namespace Render::GL
