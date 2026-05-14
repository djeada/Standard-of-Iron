#include "boulder_renderer.h"

#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "../scene_renderer.h"
#include "decoration_gpu.h"
#include "ground_utils.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "scatter_composition.h"
#include "scatter_runtime.h"
#include "spawn_validator.h"

namespace {

using std::uint32_t;
using namespace Render::Ground;

} // namespace

namespace Render::GL {

BoulderRenderer::BoulderRenderer() = default;
BoulderRenderer::~BoulderRenderer() = default;

void BoulderRenderer::configure(const Game::Map::TerrainHeightMap& height_map,
                                const Game::Map::BiomeSettings& biome_settings,
                                const std::vector<Game::Map::WorldProp>& world_props) {
  m_biome_settings = biome_settings;
  m_state.reset_instances();
  m_state.params.light_direction = m_light_direction;
  generate_instances(world_props, height_map);
}

void BoulderRenderer::set_light_direction(const QVector3D& dir) {
  m_light_direction =
      dir.isNull() ? StoneBatchParams::default_light_direction() : dir.normalized();
  m_state.params.light_direction = m_light_direction;
}

void BoulderRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  Q_UNUSED(resources);

  const auto visible_count = Scatter::sync_filtered_state(
      m_state,
      [](const StoneInstanceGpu& inst) -> const QVector4D& { return inst.pos_scale; });
  if (visible_count == 0 || !m_state.instance_buffer) {
    return;
  }

  TerrainScatterCmd cmd;
  cmd.species = TerrainScatterCmd::Species::Stone;
  cmd.instance_buffer = m_state.instance_buffer.get();
  cmd.instance_count = visible_count;
  cmd.stone = m_state.params;
  renderer.terrain_scatter(cmd);
}

void BoulderRenderer::clear() {
  m_state.reset_instances();
}

void BoulderRenderer::generate_instances(
    const std::vector<Game::Map::WorldProp>& world_props,
    const Game::Map::TerrainHeightMap& height_map) {

  auto& terrain_service = Game::Map::TerrainService::instance();
  const float tile_size = height_map.get_tile_size();
  const int width = height_map.get_width();
  const int map_height = height_map.get_height();
  const float half_w = static_cast<float>(width) * 0.5F;
  const float half_h = static_cast<float>(map_height) * 0.5F;

  const auto surface_profile = Game::Map::make_surface_profile(m_biome_settings);
  const auto scatter_profile = Game::Map::make_scatter_profile(m_biome_settings);

  SpawnTerrainCache terrain_cache;
  terrain_cache.build_from_height_map(height_map.get_height_data(),
                                      height_map.getTerrainTypes(),
                                      width,
                                      map_height,
                                      tile_size);

  SpawnValidationConfig config = make_stone_spawn_config();
  config.grid_width = width;
  config.grid_height = map_height;
  config.tile_size = tile_size;
  config.edge_padding = scatter_profile.spawn_edge_padding * 0.45F;
  config.max_slope = 0.38F;
  config.allow_hill = true;
  config.road_clearance = 1.8F;
  config.river_clearance = 1.4F;

  SpawnValidator validator(terrain_cache, config);
  ScatterCompositionContext composition(
      terrain_cache, width, map_height, tile_size, m_biome_settings, world_props);

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
    QVector3D const brown_tint(0.45F, 0.38F, 0.30F);
    float const brown_mix = remap(rand_01(state), 0.08F, 0.28F + scene.dryness * 0.12F);
    color = color * (1.0F - brown_mix) + brown_tint * brown_mix;

    StoneInstanceGpu inst;
    float const scale = remap(rand_01(state), scale_min, scale_max) *
                        scatter_scale_bias(ScatterRuleSpecies::Stone, scene);
    inst.pos_scale = QVector4D(world_x, world_y + 0.01F, world_z, scale);
    inst.color_rot = QVector4D(
        color.x(), color.y(), color.z(), rand_01(state) * MathConstants::k_two_pi);
    m_state.instances.push_back(inst);
    return true;
  };

  for (const auto& prop : world_props) {
    if (prop.type != Game::Map::WorldProp::Type::Boulder) {
      continue;
    }
    const float wx = (prop.x - half_w) * tile_size;
    const float wz = (prop.z - half_h) * tile_size;
    const QVector3D resolved =
        terrain_service.resolve_surface_world_position(wx, wz, 0.0F, 0.0F);

    uint32_t state = hash_coords(static_cast<int>(prop.x),
                                 static_cast<int>(prop.z),
                                 m_biome_settings.seed ^ 0xD3A4B1C2U);
    float const color_var = remap(rand_01(state), 0.0F, 1.0F);
    QVector3D const base_rock = surface_profile.rock_low;
    QVector3D const high_rock = surface_profile.rock_high;
    QVector3D color = base_rock * (1.0F - color_var) + high_rock * color_var;
    float const brown_mix = remap(rand_01(state), 0.0F, 0.35F);
    QVector3D const brown_tint(0.45F, 0.38F, 0.30F);
    color = color * (1.0F - brown_mix) + brown_tint * brown_mix;

    StoneInstanceGpu inst;
    inst.pos_scale = QVector4D(resolved.x(),
                               resolved.y(),
                               resolved.z(),
                               prop.scale * Game::Map::world_prop_render_scale(
                                                Game::Map::WorldProp::Type::Boulder));
    inst.color_rot = QVector4D(color.x(), color.y(), color.z(), prop.rotation);
    m_state.instances.push_back(inst);
  }

  if (width >= 2 && map_height >= 2 && !height_map.get_height_data().empty()) {
    float const base_density =
        std::clamp(0.090F + m_biome_settings.rock_exposure * 0.120F -
                       m_biome_settings.moisture_level * 0.020F,
                   0.055F,
                   0.200F);
    for (int z = 0; z < map_height; z += 6) {
      for (int x = 0; x < width; x += 6) {
        int const sample_x = std::min(x + 3, width - 1);
        int const sample_z = std::min(z + 3, map_height - 1);
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
        float const gx = static_cast<float>(x) + rand_01(state) * 6.0F;
        float const gz = static_cast<float>(z) + rand_01(state) * 6.0F;
        add_boulder(gx, gz, 1.15F, 2.45F, state);
      }
    }
  }

  m_state.instance_count = m_state.instances.size();
  m_state.instances_dirty = m_state.instance_count > 0;
}

} // namespace Render::GL
