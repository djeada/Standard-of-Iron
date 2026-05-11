#include "terrain_scatter_manager.h"

#include "../../game/map/map_definition.h"
#include "../../game/map/terrain_service.h"
#include "../ground/biome_renderer.h"
#include "../ground/dead_tree_renderer.h"
#include "../ground/firecamp_renderer.h"
#include "../ground/olive_renderer.h"
#include "../ground/pine_renderer.h"
#include "../ground/plant_renderer.h"
#include "../ground/ruins_renderer.h"
#include "../ground/stone_renderer.h"
#include "../ground/supply_cart_renderer.h"
#include "../ground/tent_renderer.h"
#include "../ground/weapon_rack_renderer.h"
#include <QVector3D>
#include <tuple>
#include <utility>

namespace Render::GL {

namespace {

auto convert_fire_camps(const std::vector<Game::Map::FireCamp> &fire_camps,
                        const Game::Map::TerrainHeightMap &height_map)
    -> std::tuple<std::vector<QVector3D>, std::vector<float>,
                  std::vector<float>> {
  std::vector<QVector3D> positions;
  std::vector<float> intensities;
  std::vector<float> radii;

  positions.reserve(fire_camps.size());
  intensities.reserve(fire_camps.size());
  radii.reserve(fire_camps.size());

  const float tile_size = height_map.get_tile_size();
  const int width = height_map.get_width();
  const int height = height_map.get_height();
  const float half_width = width * 0.5F;
  const float half_height = height * 0.5F;
  auto &terrain_service = Game::Map::TerrainService::instance();

  for (const auto &fire_camp : fire_camps) {
    const float world_x = (fire_camp.x - half_width) * tile_size;
    const float world_z = (fire_camp.z - half_height) * tile_size;
    positions.push_back(terrain_service.resolve_surface_world_position(
        world_x, world_z, 0.0F, 0.0F));
    intensities.push_back(fire_camp.intensity);
    radii.push_back(fire_camp.radius);
  }

  return {std::move(positions), std::move(intensities), std::move(radii)};
}

} // namespace

TerrainScatterManager::TerrainScatterManager()
    : m_biome(std::make_unique<BiomeRenderer>()),
      m_stone(std::make_unique<StoneRenderer>()),
      m_plant(std::make_unique<PlantRenderer>()),
      m_pine(std::make_unique<PineRenderer>()),
      m_olive(std::make_unique<OliveRenderer>()),
      m_firecamp(std::make_unique<FireCampRenderer>()),
      m_tent(std::make_unique<TentRenderer>()),
      m_supply_cart(std::make_unique<SupplyCartRenderer>()),
      m_weapon_rack(std::make_unique<WeaponRackRenderer>()),
      m_ruins(std::make_unique<RuinsRenderer>()),
      m_dead_tree(std::make_unique<DeadTreeRenderer>()),
      m_passes{m_biome.get(), m_stone.get(),       m_plant.get(),
               m_pine.get(),  m_olive.get(),        m_firecamp.get(),
               m_tent.get(),  m_supply_cart.get(),  m_weapon_rack.get(),
               m_ruins.get(), m_dead_tree.get()} {}

TerrainScatterManager::~TerrainScatterManager() = default;

void TerrainScatterManager::configure(
    const Game::Map::TerrainHeightMap &height_map,
    const Game::Map::BiomeSettings &biome_settings,
    const std::vector<Game::Map::FireCamp> &fire_camps,
    const std::vector<Game::Map::WorldProp> &world_props) {
  std::lock_guard<std::mutex> lock(m_mutex);

  m_biome->configure(height_map, biome_settings);
  m_stone->configure(height_map, biome_settings);
  m_plant->configure(height_map, biome_settings);
  m_pine->configure(height_map, biome_settings);
  m_olive->configure(height_map, biome_settings);
  m_firecamp->configure(height_map, biome_settings);
  m_tent->configure(height_map, biome_settings, fire_camps, world_props);
  m_supply_cart->configure(height_map, biome_settings, fire_camps, world_props);
  m_weapon_rack->configure(height_map, biome_settings, fire_camps, world_props);
  m_ruins->configure(height_map, biome_settings, fire_camps, world_props);
  m_dead_tree->configure(height_map, biome_settings, fire_camps, world_props);

  const auto [positions, intensities, radii] =
      convert_fire_camps(fire_camps, height_map);
  m_firecamp->set_explicit_fire_camps(positions, intensities, radii);
}

void TerrainScatterManager::submit(Renderer &renderer,
                                   ResourceManager *resources) {
  std::lock_guard<std::mutex> lock(m_mutex);

  for (auto *pass : m_passes) {
    if (pass != nullptr) {
      pass->submit(renderer, resources);
    }
  }
}

void TerrainScatterManager::clear() {
  std::lock_guard<std::mutex> lock(m_mutex);

  m_biome->clear();
  m_stone->clear();
  m_plant->clear();
  m_pine->clear();
  m_olive->clear();
  m_firecamp->clear();
  m_tent->clear();
  m_supply_cart->clear();
  m_weapon_rack->clear();
  m_ruins->clear();
  m_dead_tree->clear();
}

void TerrainScatterManager::refresh_grass() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_biome->refresh_grass();
}

auto TerrainScatterManager::is_gpu_ready() const -> bool {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_biome->is_gpu_ready() && m_stone->is_gpu_ready() &&
         m_plant->is_gpu_ready() && m_pine->is_gpu_ready() &&
         m_olive->is_gpu_ready() && m_firecamp->is_gpu_ready() &&
         m_tent->is_gpu_ready() && m_supply_cart->is_gpu_ready() &&
         m_weapon_rack->is_gpu_ready() && m_ruins->is_gpu_ready() &&
         m_dead_tree->is_gpu_ready();
}

auto TerrainScatterManager::biome() const -> BiomeRenderer * {
  return m_biome.get();
}

auto TerrainScatterManager::stone() const -> StoneRenderer * {
  return m_stone.get();
}

auto TerrainScatterManager::plant() const -> PlantRenderer * {
  return m_plant.get();
}

auto TerrainScatterManager::pine() const -> PineRenderer * {
  return m_pine.get();
}

auto TerrainScatterManager::olive() const -> OliveRenderer * {
  return m_olive.get();
}

auto TerrainScatterManager::firecamp() const -> FireCampRenderer * {
  return m_firecamp.get();
}

auto TerrainScatterManager::tent() const -> TentRenderer * {
  return m_tent.get();
}

auto TerrainScatterManager::supply_cart() const -> SupplyCartRenderer * {
  return m_supply_cart.get();
}

auto TerrainScatterManager::weapon_rack() const -> WeaponRackRenderer * {
  return m_weapon_rack.get();
}

auto TerrainScatterManager::ruins() const -> RuinsRenderer * {
  return m_ruins.get();
}

auto TerrainScatterManager::dead_tree() const -> DeadTreeRenderer * {
  return m_dead_tree.get();
}

auto TerrainScatterManager::chunks() const -> std::vector<ScatterChunk> {
  std::lock_guard<std::mutex> lock(m_mutex);

  return {{ScatterSpeciesId::Grass, ScatterVisibilityMode::InstanceFiltered,
           m_biome.get(), m_biome != nullptr ? m_biome->instance_count() : 0U,
           m_biome == nullptr || m_biome->is_gpu_ready(),
           m_biome != nullptr ? m_biome->last_sync_stats()
                              : Render::Ground::Scatter::SyncStats{}},
          {ScatterSpeciesId::Stone, ScatterVisibilityMode::InstanceFiltered,
           m_stone.get(), m_stone != nullptr ? m_stone->instance_count() : 0U,
           m_stone == nullptr || m_stone->is_gpu_ready(),
           m_stone != nullptr ? m_stone->last_sync_stats()
                              : Render::Ground::Scatter::SyncStats{}},
          {ScatterSpeciesId::Plant, ScatterVisibilityMode::InstanceFiltered,
           m_plant.get(), m_plant != nullptr ? m_plant->instance_count() : 0U,
           m_plant == nullptr || m_plant->is_gpu_ready(),
           m_plant != nullptr ? m_plant->last_sync_stats()
                              : Render::Ground::Scatter::SyncStats{}},
          {ScatterSpeciesId::Pine, ScatterVisibilityMode::InstanceFiltered,
           m_pine.get(), m_pine != nullptr ? m_pine->instance_count() : 0U,
           m_pine == nullptr || m_pine->is_gpu_ready(),
           m_pine != nullptr ? m_pine->last_sync_stats()
                             : Render::Ground::Scatter::SyncStats{}},
          {ScatterSpeciesId::Olive, ScatterVisibilityMode::InstanceFiltered,
           m_olive.get(), m_olive != nullptr ? m_olive->instance_count() : 0U,
           m_olive == nullptr || m_olive->is_gpu_ready(),
           m_olive != nullptr ? m_olive->last_sync_stats()
                              : Render::Ground::Scatter::SyncStats{}},
          {ScatterSpeciesId::FireCamp, ScatterVisibilityMode::InstanceFiltered,
           m_firecamp.get(),
           m_firecamp != nullptr ? m_firecamp->instance_count() : 0U,
           m_firecamp == nullptr || m_firecamp->is_gpu_ready(),
           m_firecamp != nullptr ? m_firecamp->last_sync_stats()
                                 : Render::Ground::Scatter::SyncStats{}},
          {ScatterSpeciesId::Tent, ScatterVisibilityMode::InstanceFiltered,
           m_tent.get(), m_tent != nullptr ? m_tent->instance_count() : 0U,
           m_tent == nullptr || m_tent->is_gpu_ready(),
           m_tent != nullptr ? m_tent->last_sync_stats()
                             : Render::Ground::Scatter::SyncStats{}},
          {ScatterSpeciesId::SupplyCart, ScatterVisibilityMode::InstanceFiltered,
           m_supply_cart.get(),
           m_supply_cart != nullptr ? m_supply_cart->instance_count() : 0U,
           m_supply_cart == nullptr || m_supply_cart->is_gpu_ready(),
           m_supply_cart != nullptr ? m_supply_cart->last_sync_stats()
                                    : Render::Ground::Scatter::SyncStats{}},
          {ScatterSpeciesId::WeaponRack, ScatterVisibilityMode::InstanceFiltered,
           m_weapon_rack.get(),
           m_weapon_rack != nullptr ? m_weapon_rack->instance_count() : 0U,
           m_weapon_rack == nullptr || m_weapon_rack->is_gpu_ready(),
           m_weapon_rack != nullptr ? m_weapon_rack->last_sync_stats()
                                    : Render::Ground::Scatter::SyncStats{}},
          {ScatterSpeciesId::Ruins, ScatterVisibilityMode::InstanceFiltered,
           m_ruins.get(),
           m_ruins != nullptr ? m_ruins->instance_count() : 0U,
           m_ruins == nullptr || m_ruins->is_gpu_ready(),
           m_ruins != nullptr ? m_ruins->last_sync_stats()
                              : Render::Ground::Scatter::SyncStats{}},
          {ScatterSpeciesId::DeadTree, ScatterVisibilityMode::InstanceFiltered,
           m_dead_tree.get(),
           m_dead_tree != nullptr ? m_dead_tree->instance_count() : 0U,
           m_dead_tree == nullptr || m_dead_tree->is_gpu_ready(),
           m_dead_tree != nullptr ? m_dead_tree->last_sync_stats()
                                  : Render::Ground::Scatter::SyncStats{}}};
}

auto TerrainScatterManager::last_sync_stats() const
    -> Render::Ground::Scatter::SyncStats {
  std::lock_guard<std::mutex> lock(m_mutex);

  Render::Ground::Scatter::SyncStats stats{};
  if (m_biome != nullptr) {
    stats += m_biome->last_sync_stats();
  }
  if (m_stone != nullptr) {
    stats += m_stone->last_sync_stats();
  }
  if (m_plant != nullptr) {
    stats += m_plant->last_sync_stats();
  }
  if (m_pine != nullptr) {
    stats += m_pine->last_sync_stats();
  }
  if (m_olive != nullptr) {
    stats += m_olive->last_sync_stats();
  }
  if (m_firecamp != nullptr) {
    stats += m_firecamp->last_sync_stats();
  }
  if (m_tent != nullptr) {
    stats += m_tent->last_sync_stats();
  }
  if (m_supply_cart != nullptr) {
    stats += m_supply_cart->last_sync_stats();
  }
  if (m_weapon_rack != nullptr) {
    stats += m_weapon_rack->last_sync_stats();
  }
  if (m_ruins != nullptr) {
    stats += m_ruins->last_sync_stats();
  }
  if (m_dead_tree != nullptr) {
    stats += m_dead_tree->last_sync_stats();
  }
  return stats;
}

auto TerrainScatterManager::passes() const
    -> const std::vector<IRenderPass *> & {
  return m_passes;
}

} // namespace Render::GL
