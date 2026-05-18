#include "iron_ore_renderer.h"

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

IronOreRenderer::IronOreRenderer() = default;
IronOreRenderer::~IronOreRenderer() = default;

void IronOreRenderer::configure(const Game::Map::TerrainHeightMap& height_map,
                                const Game::Map::BiomeSettings& biome_settings,
                                const std::vector<Game::Map::WorldProp>& world_props,
                                bool use_world_props_exclusively) {
  m_biome_settings = biome_settings;
  m_use_world_props_exclusively = use_world_props_exclusively;
  m_state.reset_instances();
  m_state.params.light_direction = m_light_direction;
  generate_instances(world_props, height_map);
}

void IronOreRenderer::set_light_direction(const QVector3D& dir) {
  m_light_direction =
      dir.isNull() ? IronOreBatchParams::default_light_direction() : dir.normalized();
  m_state.params.light_direction = m_light_direction;
}

void IronOreRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  Q_UNUSED(resources);

  const auto visible_count = Scatter::sync_filtered_state(
      m_state, [](const IronOreInstanceGpu& inst) -> const QVector4D& {
        return inst.pos_scale;
      });
  if (visible_count == 0 || !m_state.instance_buffer) {
    return;
  }

  TerrainScatterCmd cmd;
  cmd.species = TerrainScatterCmd::Species::IronOre;
  cmd.instance_buffer = m_state.instance_buffer.get();
  cmd.instance_count = visible_count;
  cmd.iron_ore = m_state.params;
  renderer.terrain_scatter(cmd);
}

void IronOreRenderer::clear() {
  m_state.reset_instances();
}

void IronOreRenderer::generate_instances(
    const std::vector<Game::Map::WorldProp>& world_props,
    const Game::Map::TerrainHeightMap& height_map) {

  auto& terrain_service = Game::Map::TerrainService::instance();
  const float tile_size = height_map.get_tile_size();
  const int width = height_map.get_width();
  const int map_height = height_map.get_height();
  const float half_w = static_cast<float>(width) * 0.5F - 0.5F;
  const float half_h = static_cast<float>(map_height) * 0.5F - 0.5F;

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
  config.edge_padding = scatter_profile.spawn_edge_padding * 1.15F;
  config.max_slope = 0.20F;
  config.allow_flat = true;
  config.allow_hill = true;
  config.allow_mountain = false;
  config.road_clearance = 3.0F;
  config.river_clearance = 3.0F;

  SpawnValidator validator(terrain_cache, config);
  ScatterCompositionContext composition(
      terrain_cache, width, map_height, tile_size, m_biome_settings, world_props);

  auto make_color = [&](const ScatterCompositionSample& scene,
                        uint32_t& state) -> QVector3D {
    float const color_var = remap(rand_01(state), 0.0F, 1.0F);
    QVector3D color = surface_profile.rock_low * (1.0F - color_var) +
                      surface_profile.rock_high * color_var;
    color *= 0.58F + scene.rockiness * 0.08F;

    QVector3D const hematite_tint(0.42F, 0.18F, 0.12F);
    QVector3D const cold_magic_tint(0.12F, 0.25F, 0.35F);
    float const iron_mix =
        remap(rand_01(state), 0.24F, 0.46F + scene.rockiness * 0.10F);
    color = color * (1.0F - iron_mix) + hematite_tint * iron_mix;
    float const magic_stain = remap(rand_01(state), 0.05F, 0.16F);
    return color * (1.0F - magic_stain) + cold_magic_tint * magic_stain;
  };

  auto add_ore = [&](float gx,
                     float gz,
                     float scale_min,
                     float scale_max,
                     uint32_t& state) -> bool {
    if (!validator.can_spawn_at_grid(gx, gz)) {
      return false;
    }

    auto const scene = composition.sample_grid(gx, gz, state ^ 0xD64E21B7U);
    float const flat_support = 1.0F - std::clamp(scene.slope / 0.20F, 0.0F, 1.0F);
    float const deposit_support =
        std::max(scene.rockiness * 0.72F,
                 std::max(scene.prop_influence * 0.55F, flat_support * 0.34F));
    if (deposit_support < 0.22F) {
      return false;
    }

    float world_x = 0.0F;
    float world_z = 0.0F;
    validator.grid_to_world(gx, gz, world_x, world_z);
    float const world_y = terrain_cache.sample_height_at(gx, gz);

    float const scale = remap(rand_01(state), scale_min, scale_max) *
                        remap(deposit_support, 0.92F, 1.18F);
    QVector3D const color = make_color(scene, state);

    IronOreInstanceGpu inst;
    inst.pos_scale = QVector4D(world_x, world_y + 0.015F, world_z, scale);
    inst.color_rot = QVector4D(
        color.x(), color.y(), color.z(), rand_01(state) * MathConstants::k_two_pi);
    m_state.instances.push_back(inst);
    return true;
  };

  for (const auto& prop : world_props) {
    if (prop.type != Game::Map::WorldProp::Type::IronOre) {
      continue;
    }
    const float wx = (prop.x - half_w) * tile_size;
    const float wz = (prop.z - half_h) * tile_size;
    const QVector3D resolved =
        terrain_service.resolve_surface_world_position(wx, wz, 0.0F, 0.0F);

    uint32_t state = hash_coords(static_cast<int>(prop.x),
                                 static_cast<int>(prop.z),
                                 m_biome_settings.seed ^ 0xB7C3D1A4U);
    auto const scene = composition.sample_world(wx, wz, state ^ 0xA39B5C2DU);
    QVector3D const color = make_color(scene, state);

    IronOreInstanceGpu inst;
    inst.pos_scale = QVector4D(resolved.x(),
                               resolved.y(),
                               resolved.z(),
                               prop.scale * Game::Map::world_prop_render_scale(
                                                Game::Map::WorldProp::Type::IronOre));
    inst.color_rot = QVector4D(color.x(), color.y(), color.z(), prop.rotation);
    m_state.instances.push_back(inst);
  }

  if (m_use_world_props_exclusively) {
    m_state.instance_count = m_state.instances.size();
    m_state.instances_dirty = m_state.instance_count > 0;
    return;
  }

  if (width >= 2 && map_height >= 2 && !height_map.get_height_data().empty()) {
    float const climate_bias =
        m_biome_settings.rock_exposure * 0.014F +
        std::max(0.0F, 0.42F - m_biome_settings.moisture_level) * 0.010F;
    float const base_density = std::clamp(0.010F + climate_bias, 0.006F, 0.030F);

    for (int z = 0; z < map_height; z += 18) {
      for (int x = 0; x < width; x += 18) {
        int const sample_x = std::min(x + 9, width - 1);
        int const sample_z = std::min(z + 9, map_height - 1);
        uint32_t state = hash_coords(x, z, m_biome_settings.seed ^ 0x92E7B54CU);
        auto const scene = composition.sample_grid(static_cast<float>(sample_x),
                                                   static_cast<float>(sample_z),
                                                   state ^ 0x3C18A6D1U);
        float const flat_support = 1.0F - std::clamp(scene.slope / 0.20F, 0.0F, 1.0F);
        float const density = base_density *
                              (0.75F + scene.rockiness * 2.15F +
                               scene.prop_influence * 1.20F + flat_support * 0.80F) *
                              (0.70F + scene.cluster_bias * 0.85F);
        if (rand_01(state) > density) {
          continue;
        }

        float const gx = static_cast<float>(x) + rand_01(state) * 18.0F;
        float const gz = static_cast<float>(z) + rand_01(state) * 18.0F;
        if (!add_ore(gx, gz, 1.25F, 2.10F, state)) {
          continue;
        }

        if (rand_01(state) < 0.18F) {
          float const angle = rand_01(state) * MathConstants::k_two_pi;
          float const radius_tiles = remap(rand_01(state), 1.8F, 3.6F);
          (void)add_ore(gx + std::cos(angle) * radius_tiles,
                        gz + std::sin(angle) * radius_tiles,
                        0.72F,
                        1.18F,
                        state);
        }
      }
    }
  }

  m_state.instance_count = m_state.instances.size();
  m_state.instances_dirty = m_state.instance_count > 0;
}

} // namespace Render::GL
