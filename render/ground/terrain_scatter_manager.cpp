#include "terrain_scatter_manager.h"

#include "../../game/map/map_definition.h"
#include "../../game/map/terrain_service.h"
#include "../ground/biome_renderer.h"
#include "../ground/boulder_renderer.h"
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

namespace Render::GL {

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
      m_boulder(std::make_unique<BoulderRenderer>()),
      m_passes{m_biome.get(), m_stone.get(),       m_plant.get(),
               m_pine.get(),  m_olive.get(),       m_firecamp.get(),
               m_tent.get(),  m_supply_cart.get(), m_weapon_rack.get(),
               m_ruins.get(), m_dead_tree.get(),   m_boulder.get()} {}

TerrainScatterManager::~TerrainScatterManager() = default;

void TerrainScatterManager::configure(
    const Game::Map::TerrainHeightMap &height_map,
    const Game::Map::BiomeSettings &biome_settings,
    const std::vector<Game::Map::WorldProp> &world_props) {
  std::lock_guard<std::mutex> lock(m_mutex);

  m_biome->configure(height_map, biome_settings);
  m_stone->configure(height_map, biome_settings, world_props);
  m_plant->configure(height_map, biome_settings, world_props);
  m_pine->configure(height_map, biome_settings, world_props);
  m_olive->configure(height_map, biome_settings, world_props);
  m_firecamp->configure(height_map, biome_settings, world_props);
  m_tent->configure(height_map, biome_settings, world_props);
  m_supply_cart->configure(height_map, biome_settings, world_props);
  m_weapon_rack->configure(height_map, biome_settings, world_props);
  m_ruins->configure(height_map, biome_settings, world_props);
  m_dead_tree->configure(height_map, biome_settings, world_props);
  m_boulder->configure(height_map, biome_settings, world_props);
}

void TerrainScatterManager::set_light_direction(const QVector3D &dir) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_biome->set_light_direction(dir);
  m_stone->set_light_direction(dir);
  m_plant->set_light_direction(dir);
  m_pine->set_light_direction(dir);
  m_olive->set_light_direction(dir);
  m_tent->set_light_direction(dir);
  m_supply_cart->set_light_direction(dir);
  m_weapon_rack->set_light_direction(dir);
  m_ruins->set_light_direction(dir);
  m_dead_tree->set_light_direction(dir);
  m_boulder->set_light_direction(dir);
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
  m_boulder->clear();
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
         m_dead_tree->is_gpu_ready() && m_boulder->is_gpu_ready();
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

auto TerrainScatterManager::boulder() const -> BoulderRenderer * {
  return m_boulder.get();
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
          {ScatterSpeciesId::SupplyCart,
           ScatterVisibilityMode::InstanceFiltered, m_supply_cart.get(),
           m_supply_cart != nullptr ? m_supply_cart->instance_count() : 0U,
           m_supply_cart == nullptr || m_supply_cart->is_gpu_ready(),
           m_supply_cart != nullptr ? m_supply_cart->last_sync_stats()
                                    : Render::Ground::Scatter::SyncStats{}},
          {ScatterSpeciesId::WeaponRack,
           ScatterVisibilityMode::InstanceFiltered, m_weapon_rack.get(),
           m_weapon_rack != nullptr ? m_weapon_rack->instance_count() : 0U,
           m_weapon_rack == nullptr || m_weapon_rack->is_gpu_ready(),
           m_weapon_rack != nullptr ? m_weapon_rack->last_sync_stats()
                                    : Render::Ground::Scatter::SyncStats{}},
          {ScatterSpeciesId::Ruins, ScatterVisibilityMode::InstanceFiltered,
           m_ruins.get(), m_ruins != nullptr ? m_ruins->instance_count() : 0U,
           m_ruins == nullptr || m_ruins->is_gpu_ready(),
           m_ruins != nullptr ? m_ruins->last_sync_stats()
                              : Render::Ground::Scatter::SyncStats{}},
          {ScatterSpeciesId::DeadTree, ScatterVisibilityMode::InstanceFiltered,
           m_dead_tree.get(),
           m_dead_tree != nullptr ? m_dead_tree->instance_count() : 0U,
           m_dead_tree == nullptr || m_dead_tree->is_gpu_ready(),
           m_dead_tree != nullptr ? m_dead_tree->last_sync_stats()
                                  : Render::Ground::Scatter::SyncStats{}},
          {ScatterSpeciesId::Boulder, ScatterVisibilityMode::InstanceFiltered,
           m_boulder.get(),
           m_boulder != nullptr ? m_boulder->instance_count() : 0U,
           m_boulder == nullptr || m_boulder->is_gpu_ready(),
           m_boulder != nullptr ? m_boulder->last_sync_stats()
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
  if (m_boulder != nullptr) {
    stats += m_boulder->last_sync_stats();
  }
  return stats;
}

auto TerrainScatterManager::passes() const
    -> const std::vector<IRenderPass *> & {
  return m_passes;
}

} // namespace Render::GL
