#include "terrain_scatter_manager.h"

#include <QVector3D>

#include <algorithm>

#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "render/graphics_settings.h"
#include "render/ground/abandoned_home_renderer.h"
#include "render/ground/biome_renderer.h"
#include "render/ground/boulder_renderer.h"
#include "render/ground/cursed_gold_vein_renderer.h"
#include "render/ground/dead_tree_renderer.h"
#include "render/ground/firecamp_renderer.h"
#include "render/ground/iron_ore_renderer.h"
#include "render/ground/magic_shrine_renderer.h"
#include "render/ground/plant_renderer.h"
#include "render/ground/ruins_renderer.h"
#include "render/ground/statue_renderer.h"
#include "render/ground/stone_renderer.h"
#include "render/ground/supply_cart_renderer.h"
#include "render/ground/tent_renderer.h"
#include "render/ground/tree_renderer.h"
#include "render/ground/weapon_rack_renderer.h"
#include "render/scene_renderer.h"

namespace Render::GL {

namespace {

auto should_use_runtime_harvest_props_exclusively(
    const std::vector<Game::Map::WorldProp>& runtime_world_props,
    bool requested_exclusive) -> bool {
  if (requested_exclusive) {
    return true;
  }
  return std::any_of(runtime_world_props.begin(),
                     runtime_world_props.end(),
                     [](const Game::Map::WorldProp& prop) {
                       return !prop.persistent &&
                              Game::Map::is_harvestable_world_prop_type(prop.type);
                     });
}

} // namespace

TerrainScatterManager::TerrainScatterManager()
    : m_biome(std::make_unique<BiomeRenderer>())
    , m_stone(std::make_unique<StoneRenderer>())
    , m_plant(std::make_unique<PlantRenderer>())
    , m_trees{std::make_unique<TreeRenderer>(Game::Map::TreeSpecies::Pine),
              std::make_unique<TreeRenderer>(Game::Map::TreeSpecies::Olive),
              std::make_unique<TreeRenderer>(Game::Map::TreeSpecies::Cypress),
              std::make_unique<TreeRenderer>(Game::Map::TreeSpecies::Palm)}
    , m_firecamp(std::make_unique<FireCampRenderer>())
    , m_tent(std::make_unique<TentRenderer>())
    , m_supply_cart(std::make_unique<SupplyCartRenderer>())
    , m_weapon_rack(std::make_unique<WeaponRackRenderer>())
    , m_ruins(std::make_unique<RuinsRenderer>())
    , m_dead_tree(std::make_unique<DeadTreeRenderer>())
    , m_boulder(std::make_unique<BoulderRenderer>())
    , m_iron_ore(std::make_unique<IronOreRenderer>())
    , m_magic_shrine(std::make_unique<MagicShrineRenderer>())
    , m_cursed_gold_vein(std::make_unique<CursedGoldVeinRenderer>())
    , m_abandoned_home(std::make_unique<AbandonedHomeRenderer>())
    , m_statue(std::make_unique<StatueRenderer>())
    , m_scatter_passes{
          {ScatterSpeciesId::Grass, m_biome.get()},
          {ScatterSpeciesId::Stone, m_stone.get()},
          {ScatterSpeciesId::Plant, m_plant.get()},
          {ScatterSpeciesId::Pine,
           m_trees[static_cast<std::size_t>(Game::Map::TreeSpecies::Pine)].get()},
          {ScatterSpeciesId::Olive,
           m_trees[static_cast<std::size_t>(Game::Map::TreeSpecies::Olive)].get()},
          {ScatterSpeciesId::Cypress,
           m_trees[static_cast<std::size_t>(Game::Map::TreeSpecies::Cypress)].get()},
          {ScatterSpeciesId::Palm,
           m_trees[static_cast<std::size_t>(Game::Map::TreeSpecies::Palm)].get()},
          {ScatterSpeciesId::FireCamp, m_firecamp.get()},
          {ScatterSpeciesId::Tent, m_tent.get()},
          {ScatterSpeciesId::SupplyCart, m_supply_cart.get()},
          {ScatterSpeciesId::WeaponRack, m_weapon_rack.get()},
          {ScatterSpeciesId::Ruins, m_ruins.get()},
          {ScatterSpeciesId::DeadTree, m_dead_tree.get()},
          {ScatterSpeciesId::Boulder, m_boulder.get()},
          {ScatterSpeciesId::IronOre, m_iron_ore.get()},
          {ScatterSpeciesId::MagicShrine, m_magic_shrine.get()},
          {ScatterSpeciesId::AbandonedHome, m_abandoned_home.get()},
          {ScatterSpeciesId::Statue, m_statue.get()},
          {ScatterSpeciesId::CursedGoldVein, m_cursed_gold_vein.get()}} {
  m_passes.reserve(m_scatter_passes.size());
  for (const auto& entry : m_scatter_passes) {
    m_passes.push_back(entry.pass);
  }
}

TerrainScatterManager::~TerrainScatterManager() = default;

void TerrainScatterManager::configure(
    const Game::Map::TerrainHeightMap& height_map,
    const Game::Map::BiomeSettings& biome_settings,
    const std::vector<Game::Map::WorldProp>& scatter_seed_world_props,
    const std::vector<Game::Map::WorldProp>& runtime_world_props,
    bool use_world_props_exclusively) {
  std::lock_guard<std::mutex> const lock(m_mutex);
  m_height_map = &height_map;
  m_biome_settings = biome_settings;
  m_scatter_seed_world_props = scatter_seed_world_props;
  m_use_world_props_exclusively = should_use_runtime_harvest_props_exclusively(
      runtime_world_props, use_world_props_exclusively);

  m_biome->configure(height_map, biome_settings);
  m_stone->configure(height_map, biome_settings, m_scatter_seed_world_props);
  m_plant->configure(height_map, biome_settings, m_scatter_seed_world_props);
  for (auto& tree_pass : m_trees) {
    tree_pass->configure(height_map,
                         biome_settings,
                         m_scatter_seed_world_props,
                         runtime_world_props,
                         m_use_world_props_exclusively);
  }
  m_firecamp->configure(height_map, biome_settings, runtime_world_props);
  m_tent->configure(height_map, biome_settings, runtime_world_props);
  m_supply_cart->configure(height_map, biome_settings, runtime_world_props);
  m_weapon_rack->configure(height_map, biome_settings, runtime_world_props);
  m_ruins->configure(height_map, biome_settings, runtime_world_props);
  m_dead_tree->configure(
      height_map, biome_settings, m_scatter_seed_world_props, runtime_world_props);
  m_boulder->configure(height_map,
                       biome_settings,
                       m_scatter_seed_world_props,
                       runtime_world_props,
                       m_use_world_props_exclusively);
  m_iron_ore->configure(height_map, biome_settings, runtime_world_props);
  m_magic_shrine->configure(height_map, biome_settings, runtime_world_props);
  m_cursed_gold_vein->configure(height_map, biome_settings, runtime_world_props);
  m_abandoned_home->configure(height_map, biome_settings, runtime_world_props);
  m_statue->configure(height_map, biome_settings, runtime_world_props);
}

void TerrainScatterManager::refresh_runtime_world_props(
    const std::vector<Game::Map::WorldProp>& runtime_world_props) {
  std::lock_guard<std::mutex> const lock(m_mutex);

  if (m_height_map == nullptr) {
    return;
  }

  m_use_world_props_exclusively =
      should_use_runtime_harvest_props_exclusively(runtime_world_props, false);

  for (auto& tree_pass : m_trees) {
    tree_pass->refresh_world_props(runtime_world_props, m_use_world_props_exclusively);
  }
  m_boulder->refresh_world_props(runtime_world_props, m_use_world_props_exclusively);
  m_dead_tree->refresh_world_props(runtime_world_props);

  m_iron_ore->configure(*m_height_map, m_biome_settings, runtime_world_props);

  m_firecamp->configure(*m_height_map, m_biome_settings, runtime_world_props);
  m_tent->configure(*m_height_map, m_biome_settings, runtime_world_props);
  m_supply_cart->configure(*m_height_map, m_biome_settings, runtime_world_props);
  m_weapon_rack->configure(*m_height_map, m_biome_settings, runtime_world_props);
  m_ruins->configure(*m_height_map, m_biome_settings, runtime_world_props);
  m_magic_shrine->configure(*m_height_map, m_biome_settings, runtime_world_props);
  m_cursed_gold_vein->configure(*m_height_map, m_biome_settings, runtime_world_props);
  m_abandoned_home->configure(*m_height_map, m_biome_settings, runtime_world_props);
  m_statue->configure(*m_height_map, m_biome_settings, runtime_world_props);
}

void TerrainScatterManager::set_light_direction(const QVector3D& dir) {
  std::lock_guard<std::mutex> const lock(m_mutex);
  for (const auto& entry : m_scatter_passes) {
    entry.pass->set_light_direction(dir);
  }
}

void TerrainScatterManager::submit(Renderer& renderer, ResourceManager* resources) {
  std::lock_guard<std::mutex> const lock(m_mutex);

  const std::uint32_t generation = GraphicsSettings::instance().generation();
  if (generation != m_applied_graphics_generation) {
    m_applied_graphics_generation = generation;
    if (m_biome != nullptr && !m_biome->grass_matches_graphics_profile()) {
      m_biome->refresh_grass();
    }
  }

  const bool previous_filter =
      Render::Ground::Scatter::visibility_filter_enabled_for_current_thread();
  Render::Ground::Scatter::set_visibility_filter_enabled_for_current_thread(
      renderer.static_world_visibility_filter_enabled());
  for (const auto& entry : m_scatter_passes) {
    entry.pass->submit(renderer, resources);
  }
  Render::Ground::Scatter::set_visibility_filter_enabled_for_current_thread(
      previous_filter);
}

void TerrainScatterManager::clear() {
  std::lock_guard<std::mutex> const lock(m_mutex);

  m_height_map = nullptr;
  m_biome_settings = Game::Map::BiomeSettings{};
  m_scatter_seed_world_props.clear();
  m_use_world_props_exclusively = false;

  for (const auto& entry : m_scatter_passes) {
    entry.pass->clear();
  }
}

void TerrainScatterManager::refresh_grass() {
  std::lock_guard<std::mutex> const lock(m_mutex);
  m_biome->refresh_grass();
}

auto TerrainScatterManager::is_gpu_ready() const -> bool {
  std::lock_guard<std::mutex> const lock(m_mutex);
  return std::all_of(
      m_scatter_passes.begin(),
      m_scatter_passes.end(),
      [](const ScatterPassEntry& entry) { return entry.pass->is_gpu_ready(); });
}

auto TerrainScatterManager::biome() const -> BiomeRenderer* {
  return m_biome.get();
}

auto TerrainScatterManager::stone() const -> StoneRenderer* {
  return m_stone.get();
}

auto TerrainScatterManager::plant() const -> PlantRenderer* {
  return m_plant.get();
}

auto TerrainScatterManager::tree(Game::Map::TreeSpecies species) const
    -> TreeRenderer* {
  return m_trees[static_cast<std::size_t>(species)].get();
}

auto TerrainScatterManager::firecamp() const -> FireCampRenderer* {
  return m_firecamp.get();
}

auto TerrainScatterManager::tent() const -> TentRenderer* {
  return m_tent.get();
}

auto TerrainScatterManager::supply_cart() const -> SupplyCartRenderer* {
  return m_supply_cart.get();
}

auto TerrainScatterManager::weapon_rack() const -> WeaponRackRenderer* {
  return m_weapon_rack.get();
}

auto TerrainScatterManager::ruins() const -> RuinsRenderer* {
  return m_ruins.get();
}

auto TerrainScatterManager::dead_tree() const -> DeadTreeRenderer* {
  return m_dead_tree.get();
}

auto TerrainScatterManager::boulder() const -> BoulderRenderer* {
  return m_boulder.get();
}

auto TerrainScatterManager::iron_ore() const -> IronOreRenderer* {
  return m_iron_ore.get();
}

auto TerrainScatterManager::magic_shrine() const -> MagicShrineRenderer* {
  return m_magic_shrine.get();
}

auto TerrainScatterManager::cursed_gold_vein() const -> CursedGoldVeinRenderer* {
  return m_cursed_gold_vein.get();
}

auto TerrainScatterManager::abandoned_home() const -> AbandonedHomeRenderer* {
  return m_abandoned_home.get();
}

auto TerrainScatterManager::statue() const -> StatueRenderer* {
  return m_statue.get();
}

auto TerrainScatterManager::chunks() const -> std::vector<ScatterChunk> {
  std::lock_guard<std::mutex> const lock(m_mutex);

  std::vector<ScatterChunk> out;
  out.reserve(m_scatter_passes.size());
  for (const auto& entry : m_scatter_passes) {
    out.push_back({entry.species,
                   ScatterVisibilityMode::InstanceFiltered,
                   entry.pass,
                   entry.pass->instance_count(),
                   entry.pass->is_gpu_ready(),
                   entry.pass->last_sync_stats()});
  }
  return out;
}

auto TerrainScatterManager::last_sync_stats() const
    -> Render::Ground::Scatter::SyncStats {
  std::lock_guard<std::mutex> const lock(m_mutex);

  Render::Ground::Scatter::SyncStats stats{};
  for (const auto& entry : m_scatter_passes) {
    stats += entry.pass->last_sync_stats();
  }
  return stats;
}

auto TerrainScatterManager::passes() const -> const std::vector<IRenderPass*>& {
  return m_passes;
}

} // namespace Render::GL
