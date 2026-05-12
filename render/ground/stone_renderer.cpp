#include "stone_renderer.h"
#include "../gl/buffer.h"
#include "../scene_renderer.h"
#include "decoration_gpu.h"
#include "gl/render_constants.h"
#include "gl/resources.h"
#include "ground_utils.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "scatter_runtime.h"
#include "spawn_validator.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QVector2D>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <qelapsedtimer.h>
#include <qglobal.h>
#include <vector>

namespace {

using std::uint32_t;
using namespace Render::Ground;

} // namespace

namespace Render::GL {

StoneRenderer::StoneRenderer() = default;
StoneRenderer::~StoneRenderer() = default;

void StoneRenderer::configure(const Game::Map::TerrainHeightMap &height_map,
                              const Game::Map::BiomeSettings &biome_settings,
                              const std::vector<Game::Map::WorldProp> &world_props) {
  m_width = height_map.get_width();
  m_height = height_map.get_height();
  m_tile_size = height_map.get_tile_size();
  m_height_data = height_map.get_height_data();
  m_terrain_types = height_map.getTerrainTypes();
  m_biome_settings = biome_settings;
  m_noise_seed = biome_settings.seed;
  m_world_props = world_props;

  m_stone_state.reset_instances();
  auto &stone_params = m_stone_state.params;

  stone_params.light_direction = m_light_direction;
  stone_params.time = 0.0F;

  generate_stone_instances();
}

void StoneRenderer::set_light_direction(const QVector3D &dir) {
  m_light_direction =
      dir.isNull() ? QVector3D(0.35F, 0.8F, 0.45F) : dir.normalized();
  m_stone_state.params.light_direction = m_light_direction;
}

void StoneRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  Q_UNUSED(resources);

  const auto visible_count = Scatter::sync_filtered_state(
      m_stone_state, [](const StoneInstanceGpu &instance) -> const QVector4D & {
        return instance.pos_scale;
      });
  if (visible_count == 0 || !m_stone_state.instance_buffer) {
    return;
  }

  TerrainScatterCmd cmd;
  cmd.species = TerrainScatterCmd::Species::Stone;
  cmd.instance_buffer = m_stone_state.instance_buffer.get();
  cmd.instance_count = visible_count;
  cmd.stone = m_stone_state.params;
  renderer.terrain_scatter(cmd);
}

void StoneRenderer::clear() { m_stone_state.reset_instances(); }

void StoneRenderer::generate_stone_instances() {
  QElapsedTimer timer;
  timer.start();

  auto &stone_instances = m_stone_state.instances;
  auto &stone_instance_count = m_stone_state.instance_count;
  auto &stone_instances_dirty = m_stone_state.instances_dirty;

  stone_instances.clear();

  if (m_width < 2 || m_height < 2 || m_height_data.empty()) {
    stone_instance_count = 0;
    stone_instances_dirty = false;
    return;
  }

  const float tile_safe = std::max(0.001F, m_tile_size);
  const auto surface_profile =
      Game::Map::make_surface_profile(m_biome_settings);
  const auto scatter_profile =
      Game::Map::make_scatter_profile(m_biome_settings);

  SpawnTerrainCache terrain_cache;
  terrain_cache.build_from_height_map(m_height_data, m_terrain_types, m_width,
                                      m_height, m_tile_size);

  SpawnValidationConfig config = make_stone_spawn_config();
  config.grid_width = m_width;
  config.grid_height = m_height;
  config.tile_size = m_tile_size;
  config.edge_padding = scatter_profile.spawn_edge_padding;

  SpawnValidator validator(terrain_cache, config);

  auto add_stone = [&](float gx, float gz, uint32_t &state) -> bool {
    if (!validator.can_spawn_at_grid(gx, gz)) {
      return false;
    }

    float const sgx = std::clamp(gx, 0.0F, float(m_width - 1));
    float const sgz = std::clamp(gz, 0.0F, float(m_height - 1));

    float world_x = 0.0F;
    float world_z = 0.0F;
    validator.grid_to_world(gx, gz, world_x, world_z);
    float const world_y = terrain_cache.sample_height_at(sgx, sgz);

    // Larger boulders: range [0.35, 0.90] instead of [0.08, 0.25]
    float const scale = remap(rand_01(state), 0.35F, 0.90F) * tile_safe;

    float const color_var = remap(rand_01(state), 0.0F, 1.0F);
    QVector3D const base_rock = surface_profile.rock_low;
    QVector3D const high_rock = surface_profile.rock_high;
    QVector3D color = base_rock * (1.0F - color_var) + high_rock * color_var;

    float const brown_mix = remap(rand_01(state), 0.0F, 0.4F);
    QVector3D const brown_tint(0.45F, 0.38F, 0.30F);
    color = color * (1.0F - brown_mix) + brown_tint * brown_mix;

    float const rotation = rand_01(state) * MathConstants::k_two_pi;

    StoneInstanceGpu instance;
    instance.pos_scale = QVector4D(world_x, world_y + 0.01F, world_z, scale);
    instance.color_rot = QVector4D(color.x(), color.y(), color.z(), rotation);
    stone_instances.push_back(instance);
    return true;
  };

  // Use a moderate density with wider spacing (stride 4 instead of 2)
  // This prevents stones from clustering too densely while still allowing groups
  const float stone_density = 0.22F;

  for (int z = 0; z < m_height; z += 4) {
    for (int x = 0; x < m_width; x += 4) {
      int const idx = z * m_width + x;

      Game::Map::TerrainType const terrain_type =
          terrain_cache.get_terrain_type_at(x, z);
      if (terrain_type != Game::Map::TerrainType::Flat) {
        continue;
      }

      float const slope = terrain_cache.get_slope_at(x, z);
      if (slope > 0.15F) {
        continue;
      }

      uint32_t state = hash_coords(
          x, z, m_noise_seed ^ 0xABCDEF12U ^ static_cast<uint32_t>(idx));

      float world_x = 0.0F;
      float world_z = 0.0F;
      validator.grid_to_world(static_cast<float>(x), static_cast<float>(z),
                              world_x, world_z);
      float const cluster_noise = value_noise(world_x * 0.025F, world_z * 0.025F,
                                              m_noise_seed ^ 0x7F3A9B2CU);

      if (cluster_noise < 0.55F) {
        continue;
      }

      int stone_count = static_cast<int>(std::floor(stone_density));
      float const frac = stone_density - float(stone_count);
      if (rand_01(state) < frac) {
        stone_count += 1;
      }

      for (int i = 0; i < stone_count; ++i) {
        // Spread stones across the 4x4 cell with larger jitter
        float const gx = float(x) + rand_01(state) * 4.0F;
        float const gz = float(z) + rand_01(state) * 4.0F;
        add_stone(gx, gz, state);
      }
    }
  }

  // Add explicit WorldProp::Boulder placements
  auto &terrain_service = Game::Map::TerrainService::instance();
  const float half_w = static_cast<float>(m_width) * 0.5F;
  const float half_h = static_cast<float>(m_height) * 0.5F;

  for (const auto &prop : m_world_props) {
    if (prop.type != Game::Map::WorldProp::Type::Boulder) {
      continue;
    }
    const float wx = (prop.x - half_w) * m_tile_size;
    const float wz = (prop.z - half_h) * m_tile_size;
    const QVector3D resolved =
        terrain_service.resolve_surface_world_position(wx, wz, 0.0F, 0.0F);

    // Use biome rock colors for explicitly placed boulders
    uint32_t state = hash_coords(static_cast<int>(prop.x),
                                 static_cast<int>(prop.z), m_noise_seed ^ 0xD3A4B1C2U);
    float const color_var = remap(rand_01(state), 0.0F, 1.0F);
    QVector3D const base_rock = surface_profile.rock_low;
    QVector3D const high_rock = surface_profile.rock_high;
    QVector3D color = base_rock * (1.0F - color_var) + high_rock * color_var;
    float const brown_mix = remap(rand_01(state), 0.0F, 0.35F);
    QVector3D const brown_tint(0.45F, 0.38F, 0.30F);
    color = color * (1.0F - brown_mix) + brown_tint * brown_mix;

    StoneInstanceGpu inst;
    inst.pos_scale =
        QVector4D(resolved.x(), resolved.y(), resolved.z(), prop.scale);
    inst.color_rot =
        QVector4D(color.x(), color.y(), color.z(), prop.rotation);
    stone_instances.push_back(inst);
  }

  stone_instance_count = stone_instances.size();
  stone_instances_dirty = stone_instance_count > 0;
}

} // namespace Render::GL
