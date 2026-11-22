#include "render/ground/terrain_scatter_manager.h"

#include <QThreadPool>
#include <QVector3D>

#include <algorithm>
#include <utility>

#include "game/map/map_definition.h"
#include "game/map/scatter/scatter_world.h"
#include "game/map/terrain_service.h"
#include "render/frame/frame_submitter.h"
#include "render/frame/graphics_settings.h"
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
#include "render/ground/scatter/scatter_budget.h"
#include "render/ground/statue_renderer.h"
#include "render/ground/stone_renderer.h"
#include "render/ground/supply_cart_renderer.h"
#include "render/ground/tent_renderer.h"
#include "render/ground/tree_renderer.h"
#include "render/ground/weapon_rack_renderer.h"

namespace Render::GL {

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
    , m_scatter_sources{
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
  m_sources.reserve(m_scatter_sources.size());
  for (const auto& entry : m_scatter_sources) {
    m_sources.push_back(entry.source);
  }
}

// A running job owns only its captured inputs and its RefreshRun, so it may
// finish after the manager is gone; nothing adopts it then.
TerrainScatterManager::~TerrainScatterManager() = default;

void TerrainScatterManager::configure(
    const Game::Map::TerrainHeightMap& height_map,
    const Game::Map::BiomeSettings& biome_settings,
    const std::vector<Game::Map::WorldProp>& scatter_seed_world_props,
    const std::vector<Game::Map::WorldProp>& runtime_world_props) {
  std::lock_guard<std::mutex> const lock(m_mutex);
  forget_scheduled_locked();
  m_height_map = &height_map;
  m_biome_settings = biome_settings;
  m_scatter_seed_world_props = scatter_seed_world_props;
  apply_instance_budgets();

  m_biome->configure(height_map, biome_settings);
  m_stone->configure(height_map, biome_settings, m_scatter_seed_world_props);
  m_plant->configure(height_map, biome_settings, m_scatter_seed_world_props);
  for (auto& tree_pass : m_trees) {
    tree_pass->assign_anchored_trees(m_anchored_trees);
    tree_pass->configure(height_map, biome_settings, runtime_world_props);
  }
  m_firecamp->configure(height_map, biome_settings, runtime_world_props);
  m_tent->configure(height_map, biome_settings, runtime_world_props);
  m_supply_cart->configure(height_map, biome_settings, runtime_world_props);
  m_weapon_rack->configure(height_map, biome_settings, runtime_world_props);
  m_ruins->configure(height_map, biome_settings, runtime_world_props);
  m_dead_tree->configure(height_map, biome_settings, runtime_world_props);
  m_boulder->configure(height_map, biome_settings, runtime_world_props);
  m_iron_ore->configure(height_map, biome_settings, runtime_world_props);
  m_magic_shrine->configure(height_map, biome_settings, runtime_world_props);
  m_cursed_gold_vein->configure(height_map, biome_settings, runtime_world_props);
  m_abandoned_home->configure(height_map, biome_settings, runtime_world_props);
  m_statue->configure(height_map, biome_settings, runtime_world_props);
}

void TerrainScatterManager::apply_instance_budgets() {
  const std::size_t grid_cells =
      m_height_map == nullptr
          ? 0U
          : static_cast<std::size_t>(std::max(m_height_map->get_width(), 0)) *
                static_cast<std::size_t>(std::max(m_height_map->get_height(), 0));
  const float grass_density = GraphicsSettings::instance().profile().grass_density;
  for (const auto& entry : m_scatter_sources) {
    entry.source->set_instance_budget(Render::Ground::Scatter::scatter_instance_budget(
        entry.species, grid_cells, grass_density));
  }
}

void TerrainScatterManager::refresh_runtime_world_props(
    const std::vector<Game::Map::WorldProp>& runtime_world_props) {
  std::lock_guard<std::mutex> const lock(m_mutex);
  // A synchronous refresh supersedes whatever was scheduled.
  forget_scheduled_locked();

  if (m_height_map == nullptr) {
    return;
  }

  for (auto& tree_pass : m_trees) {
    tree_pass->refresh_world_props(runtime_world_props);
  }
  m_boulder->refresh_world_props(runtime_world_props);
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

void TerrainScatterManager::set_anchored_trees(const std::vector<AnchoredTree>& trees) {
  std::lock_guard<std::mutex> const lock(m_mutex);
  forget_scheduled_locked();
  m_anchored_trees = trees;
  if (m_height_map == nullptr) {
    return;
  }
  for (auto& tree_pass : m_trees) {
    tree_pass->set_anchored_trees(m_anchored_trees);
  }
}

void TerrainScatterManager::set_light_direction(const QVector3D& dir) {
  std::lock_guard<std::mutex> const lock(m_mutex);
  for (const auto& entry : m_scatter_sources) {
    entry.source->set_light_direction(dir);
  }
}

void TerrainScatterManager::submit(FrameSubmitter& renderer,
                                   const DrawResources* resources) {
  std::lock_guard<std::mutex> const lock(m_mutex);

  // The one swap: finished regenerations replace the layers' instances here,
  // before anything below syncs or submits them.
  adopt_finished_locked();

  const std::uint32_t generation = GraphicsSettings::instance().generation();
  if (generation != m_applied_graphics_generation) {
    m_applied_graphics_generation = generation;
    if (m_biome != nullptr && !m_biome->grass_matches_graphics_profile()) {
      apply_instance_budgets();
      if (m_snapshots_provided) {
        // Generating grass reads the live map, which this thread must not;
        // ask the map's owner for a snapshot and regenerate on a worker.
        m_pending.grass_density = GraphicsSettings::instance().profile().grass_density;
        m_wants_world_snapshot = true;
      } else {
        m_biome->refresh_grass();
      }
    }
  }

  const bool previous_filter =
      Render::Ground::Scatter::visibility_filter_enabled_for_current_thread();
  Render::Ground::Scatter::set_visibility_filter_enabled_for_current_thread(
      renderer.static_world_visibility_filter_enabled());
  for (const auto& entry : m_scatter_sources) {
    entry.source->submit(renderer, resources);
  }
  Render::Ground::Scatter::set_visibility_filter_enabled_for_current_thread(
      previous_filter);
}

void TerrainScatterManager::prepare_instance_blocks() {
  std::lock_guard<std::mutex> const lock(m_mutex);
  for (const auto& entry : m_scatter_sources) {
    entry.source->prepare_instance_blocks();
  }
}

void TerrainScatterManager::for_each_instance_block(
    const InstanceBlockVisitor& visit) const {
  std::lock_guard<std::mutex> const lock(m_mutex);
  for (const auto& entry : m_scatter_sources) {
    entry.source->for_each_instance_block(visit);
  }
}

void TerrainScatterManager::clear() {
  std::lock_guard<std::mutex> const lock(m_mutex);
  forget_scheduled_locked();

  m_height_map = nullptr;
  m_biome_settings = Game::Map::BiomeSettings{};
  m_scatter_seed_world_props.clear();

  for (const auto& entry : m_scatter_sources) {
    entry.source->clear();
  }
}

void TerrainScatterManager::refresh_grass() {
  std::lock_guard<std::mutex> const lock(m_mutex);
  m_biome->refresh_grass();
}

void TerrainScatterManager::schedule_runtime_world_props_refresh(
    std::vector<Game::Map::WorldProp> runtime_world_props,
    std::shared_ptr<const Game::Map::ScatterWorld> world) {
  std::lock_guard<std::mutex> const lock(m_mutex);
  m_snapshots_provided = true;
  if (m_height_map == nullptr || world == nullptr) {
    return;
  }
  m_pending.world_props = std::move(runtime_world_props);
  m_pending_world = std::move(world);
  launch_pending_locked();
}

void TerrainScatterManager::schedule_anchored_trees(
    std::vector<AnchoredTree> trees,
    std::shared_ptr<const Game::Map::ScatterWorld> world) {
  std::lock_guard<std::mutex> const lock(m_mutex);
  m_snapshots_provided = true;
  m_anchored_trees = trees;
  if (m_height_map == nullptr || world == nullptr) {
    return;
  }
  m_pending.anchored_trees = std::move(trees);
  m_pending_world = std::move(world);
  launch_pending_locked();
}

auto TerrainScatterManager::wants_world_snapshot() const -> bool {
  std::lock_guard<std::mutex> const lock(m_mutex);
  return m_wants_world_snapshot;
}

void TerrainScatterManager::provide_world_snapshot(
    std::shared_ptr<const Game::Map::ScatterWorld> world) {
  std::lock_guard<std::mutex> const lock(m_mutex);
  m_snapshots_provided = true;
  m_wants_world_snapshot = false;
  if (world == nullptr) {
    return;
  }
  m_pending_world = std::move(world);
  launch_pending_locked();
}

void TerrainScatterManager::finish_scheduled_refreshes() {
  std::lock_guard<std::mutex> const lock(m_mutex);
  while (m_running != nullptr) {
    {
      std::unique_lock<std::mutex> run_lock(m_running->mutex);
      m_running->finished.wait(run_lock, [this] { return m_running->done; });
    }
    adopt_finished_locked();
  }
}

auto TerrainScatterManager::has_scheduled_refreshes() const -> bool {
  std::lock_guard<std::mutex> const lock(m_mutex);
  return m_running != nullptr || !m_pending.empty();
}

void TerrainScatterManager::launch_pending_locked() {
  if (m_pending.empty() || m_pending_world == nullptr || m_height_map == nullptr) {
    return;
  }
  if (m_running != nullptr) {
    // One job at a time: these wait for the running one and then take the
    // newest snapshot.
    return;
  }

  // Plan on this thread, which holds m_mutex, so every plan copies a
  // consistent view of its layer; the jobs touch nothing but those copies.
  std::vector<Render::Ground::Scatter::ScatterJob> jobs;
  const auto& world = m_pending_world;
  if (m_pending.world_props.has_value() || m_pending.anchored_trees.has_value()) {
    const auto* props =
        m_pending.world_props.has_value() ? &*m_pending.world_props : nullptr;
    const auto* anchored =
        m_pending.anchored_trees.has_value() ? &*m_pending.anchored_trees : nullptr;
    for (auto& tree_pass : m_trees) {
      jobs.push_back(tree_pass->plan_rebuild(props, anchored, world));
    }
  }
  if (m_pending.world_props.has_value()) {
    const auto& props = *m_pending.world_props;
    jobs.push_back(m_boulder->plan_world_props_refresh(props, world));
    jobs.push_back(m_dead_tree->plan_world_props_refresh(props, world));
    jobs.push_back(m_iron_ore->plan_world_props_refresh(props, world));
    jobs.push_back(m_firecamp->plan_world_props_refresh(props, world));
    jobs.push_back(m_tent->plan_world_props_refresh(props, world));
    jobs.push_back(m_supply_cart->plan_world_props_refresh(props, world));
    jobs.push_back(m_weapon_rack->plan_world_props_refresh(props, world));
    jobs.push_back(m_ruins->plan_world_props_refresh(props, world));
    jobs.push_back(m_magic_shrine->plan_world_props_refresh(props, world));
    jobs.push_back(m_cursed_gold_vein->plan_world_props_refresh(props, world));
    jobs.push_back(m_abandoned_home->plan_world_props_refresh(props, world));
    jobs.push_back(m_statue->plan_world_props_refresh(props, world));
  }
  if (m_pending.grass_density.has_value()) {
    jobs.push_back(m_biome->plan_grass(*m_pending.grass_density, world));
  }
  m_pending = {};
  m_pending_world.reset();

  auto run = std::make_shared<RefreshRun>();
  run->epoch = m_epoch;
  m_running = run;
  // The jobs share one snapshot, whose terrain copy keeps a private lookup
  // cache, so they run one after another in a single task.
  QThreadPool::globalInstance()->start([run, jobs = std::move(jobs)]() mutable {
    std::vector<Render::Ground::Scatter::ScatterAdoption> adoptions;
    adoptions.reserve(jobs.size());
    for (auto& job : jobs) {
      adoptions.push_back(job());
    }
    std::lock_guard<std::mutex> const run_lock(run->mutex);
    run->adoptions = std::move(adoptions);
    run->done = true;
    run->finished.notify_all();
  });
}

void TerrainScatterManager::adopt_finished_locked() {
  if (m_running == nullptr) {
    return;
  }
  std::vector<Render::Ground::Scatter::ScatterAdoption> adoptions;
  {
    std::lock_guard<std::mutex> const run_lock(m_running->mutex);
    if (!m_running->done) {
      return;
    }
    if (m_running->epoch == m_epoch) {
      adoptions = std::move(m_running->adoptions);
    }
  }
  m_running.reset();
  for (auto& adopt : adoptions) {
    adopt();
  }
  launch_pending_locked();
}

void TerrainScatterManager::forget_scheduled_locked() {
  ++m_epoch;
  m_pending = {};
  m_pending_world.reset();
  m_wants_world_snapshot = false;
  // A job still running finishes on its own; its epoch no longer matches, so
  // nothing adopts it.
  m_running.reset();
}

auto TerrainScatterManager::is_gpu_ready() const -> bool {
  std::lock_guard<std::mutex> const lock(m_mutex);
  return std::all_of(
      m_scatter_sources.begin(),
      m_scatter_sources.end(),
      [](const ScatterSourceEntry& entry) { return entry.source->is_gpu_ready(); });
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
  out.reserve(m_scatter_sources.size());
  for (const auto& entry : m_scatter_sources) {
    out.push_back({entry.species,
                   ScatterVisibilityMode::InstanceFiltered,
                   entry.source,
                   entry.source->instance_count(),
                   entry.source->is_gpu_ready(),
                   entry.source->last_sync_stats()});
  }
  return out;
}

auto TerrainScatterManager::memory() const -> std::vector<LayerMemory> {
  std::lock_guard<std::mutex> const lock(m_mutex);
  std::vector<LayerMemory> out;
  out.reserve(m_scatter_sources.size());
  for (const auto& entry : m_scatter_sources) {
    LayerMemory layer;
    layer.species = entry.species;
    layer.instances = entry.source->instance_count();
    layer.cpu_bytes = entry.source->instance_bytes();
    layer.budget = entry.source->instance_budget();
    layer.budget_hit = entry.source->budget_hit();
    layer.instance_stride = entry.source->instance_stride();
    entry.source->for_each_instance_block([&layer](const InstanceBlock& block) {
      layer.gpu_reserved_bytes += block.reserved_bytes();
    });
    out.push_back(layer);
  }
  return out;
}

auto TerrainScatterManager::last_sync_stats() const
    -> Render::Ground::Scatter::SyncStats {
  std::lock_guard<std::mutex> const lock(m_mutex);

  Render::Ground::Scatter::SyncStats stats{};
  for (const auto& entry : m_scatter_sources) {
    stats += entry.source->last_sync_stats();
  }
  return stats;
}

auto TerrainScatterManager::sources() const -> const std::vector<IDrawSource*>& {
  return m_sources;
}

} // namespace Render::GL
