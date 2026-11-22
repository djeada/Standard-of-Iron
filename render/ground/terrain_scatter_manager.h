#pragma once

#include <QVector3D>

#include <array>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "game/map/biome_settings.h"
#include "game/map/map_definition.h"
#include "game/map/terrain.h"
#include "render/frame/terrain_scene_types.h"
#include "render/ground/i_scatter_source.h"
#include "render/ground/scatter/scatter_request.h"

namespace Game::Map {
class ScatterWorld;
}

namespace Render::GL {

class BiomeRenderer;
class BoulderRenderer;
class DeadTreeRenderer;
class FireCampRenderer;
class IronOreRenderer;
class MagicShrineRenderer;
class CursedGoldVeinRenderer;
class AbandonedHomeRenderer;
class StatueRenderer;
class PlantRenderer;
class RuinsRenderer;
class StoneRenderer;
class SupplyCartRenderer;
class TentRenderer;
class TreeRenderer;
struct AnchoredTree;
class WeaponRackRenderer;

class TerrainScatterManager : public IDrawSource {
public:
  TerrainScatterManager();
  ~TerrainScatterManager() override;

  void configure(const Game::Map::TerrainHeightMap& height_map,
                 const Game::Map::BiomeSettings& biome_settings,
                 const std::vector<Game::Map::WorldProp>& scatter_seed_world_props = {},
                 const std::vector<Game::Map::WorldProp>& runtime_world_props = {});
  // Synchronous refreshes: generate on the calling thread against the live
  // services. Only for a thread that owns the map (tools, tests, load).
  void refresh_runtime_world_props(
      const std::vector<Game::Map::WorldProp>& runtime_world_props);
  void set_anchored_trees(const std::vector<AnchoredTree>& trees);

  // Asynchronous refreshes (the game). The caller captures `world` on the
  // thread that owns the map; the layers regenerate on a worker against it,
  // and the next submit after the worker finishes adopts the result in one
  // swap. Until then every layer keeps drawing its previous instances. Newer
  // requests made while a job runs are merged and run against the newest
  // snapshot once it finishes.
  void schedule_runtime_world_props_refresh(
      std::vector<Game::Map::WorldProp> runtime_world_props,
      std::shared_ptr<const Game::Map::ScatterWorld> world);
  void schedule_anchored_trees(std::vector<AnchoredTree> trees,
                               std::shared_ptr<const Game::Map::ScatterWorld> world);
  // A grass regeneration after a graphics-profile change is detected on the
  // render thread, which must not read the live map; it asks the owner of the
  // map for a snapshot instead. The owner checks this at its sync point and
  // answers with provide_world_snapshot().
  [[nodiscard]] auto wants_world_snapshot() const -> bool;
  void provide_world_snapshot(std::shared_ptr<const Game::Map::ScatterWorld> world);
  // Blocks until every scheduled refresh has run and adopts the results (tests,
  // and a caller that must not draw the old instances again).
  void finish_scheduled_refreshes();
  [[nodiscard]] auto has_scheduled_refreshes() const -> bool;

  void set_light_direction(const QVector3D& dir);

  void submit(FrameSubmitter& renderer, const DrawResources* resources) override;

  void clear();
  void refresh_grass();

  [[nodiscard]] bool is_gpu_ready() const;
  // Partitions every layer and reserves its instance blocks (load time).
  void prepare_instance_blocks();
  // Every instance block of every layer, for prewarming a backend.
  void for_each_instance_block(const InstanceBlockVisitor& visit) const;

  [[nodiscard]] auto biome() const -> BiomeRenderer*;
  [[nodiscard]] auto stone() const -> StoneRenderer*;
  [[nodiscard]] auto plant() const -> PlantRenderer*;
  [[nodiscard]] auto tree(Game::Map::TreeSpecies species) const -> TreeRenderer*;
  [[nodiscard]] auto firecamp() const -> FireCampRenderer*;
  [[nodiscard]] auto tent() const -> TentRenderer*;
  [[nodiscard]] auto supply_cart() const -> SupplyCartRenderer*;
  [[nodiscard]] auto weapon_rack() const -> WeaponRackRenderer*;
  [[nodiscard]] auto ruins() const -> RuinsRenderer*;
  [[nodiscard]] auto dead_tree() const -> DeadTreeRenderer*;
  [[nodiscard]] auto boulder() const -> BoulderRenderer*;
  [[nodiscard]] auto iron_ore() const -> IronOreRenderer*;
  [[nodiscard]] auto magic_shrine() const -> MagicShrineRenderer*;
  [[nodiscard]] auto cursed_gold_vein() const -> CursedGoldVeinRenderer*;
  [[nodiscard]] auto abandoned_home() const -> AbandonedHomeRenderer*;
  [[nodiscard]] auto statue() const -> StatueRenderer*;
  [[nodiscard]] auto chunks() const -> std::vector<ScatterChunk>;
  [[nodiscard]] auto last_sync_stats() const -> Render::Ground::Scatter::SyncStats;
  [[nodiscard]] auto sources() const -> const std::vector<IDrawSource*>&;

  void set_world_view(const Render::WorldView& view) noexcept override {
    IDrawSource::set_world_view(view);
    for (auto* source : sources()) {
      if (source != nullptr) {
        source->set_world_view(view);
      }
    }
  }

  // What every scatter layer holds: placed instances and their CPU bytes, and
  // the GPU bytes reserved for their instance blocks.
  struct LayerMemory {
    ScatterSpeciesId species{ScatterSpeciesId::Grass};
    std::size_t instances{0};
    std::size_t cpu_bytes{0};
    std::size_t gpu_reserved_bytes{0};
    // The layer's instance budget, whether the generator reached it, and the
    // bytes one instance takes (budget x stride bounds the layer's memory).
    std::size_t budget{0};
    bool budget_hit{false};
    std::size_t instance_stride{0};
  };
  [[nodiscard]] auto memory() const -> std::vector<LayerMemory>;

private:
  // Hands every layer its share of the map's instance budget (scatter_budget.h).
  void apply_instance_budgets();

  // Requests merged until a worker can take them.
  struct PendingRefresh {
    std::optional<std::vector<Game::Map::WorldProp>> world_props;
    std::optional<std::vector<AnchoredTree>> anchored_trees;
    std::optional<float> grass_density;
    [[nodiscard]] auto empty() const -> bool {
      return !world_props.has_value() && !anchored_trees.has_value() &&
             !grass_density.has_value();
    }
  };
  // One job running on a worker; `epoch` drops it if the map changed meanwhile.
  struct RefreshRun {
    std::mutex mutex;
    std::condition_variable finished;
    bool done = false;
    std::uint64_t epoch = 0;
    std::vector<Render::Ground::Scatter::ScatterAdoption> adoptions;
  };

  // All three expect m_mutex held.
  void launch_pending_locked();
  void adopt_finished_locked();
  void forget_scheduled_locked();

  struct ScatterSourceEntry {
    ScatterSpeciesId species{ScatterSpeciesId::Grass};
    IScatterSource* source{nullptr};
  };

  std::unique_ptr<BiomeRenderer> m_biome;
  std::unique_ptr<StoneRenderer> m_stone;
  std::unique_ptr<PlantRenderer> m_plant;
  std::array<std::unique_ptr<TreeRenderer>, Game::Map::k_tree_species_count> m_trees;
  std::unique_ptr<FireCampRenderer> m_firecamp;
  std::unique_ptr<TentRenderer> m_tent;
  std::unique_ptr<SupplyCartRenderer> m_supply_cart;
  std::unique_ptr<WeaponRackRenderer> m_weapon_rack;
  std::unique_ptr<RuinsRenderer> m_ruins;
  std::unique_ptr<DeadTreeRenderer> m_dead_tree;
  std::unique_ptr<BoulderRenderer> m_boulder;
  std::unique_ptr<IronOreRenderer> m_iron_ore;
  std::unique_ptr<MagicShrineRenderer> m_magic_shrine;
  std::unique_ptr<CursedGoldVeinRenderer> m_cursed_gold_vein;
  std::unique_ptr<AbandonedHomeRenderer> m_abandoned_home;
  std::unique_ptr<StatueRenderer> m_statue;
  std::vector<ScatterSourceEntry> m_scatter_sources;
  std::vector<AnchoredTree> m_anchored_trees;
  std::vector<IDrawSource*> m_sources;
  std::uint32_t m_applied_graphics_generation{0U};
  const Game::Map::TerrainHeightMap* m_height_map = nullptr;
  Game::Map::BiomeSettings m_biome_settings;
  std::vector<Game::Map::WorldProp> m_scatter_seed_world_props;
  mutable std::mutex m_mutex;

  PendingRefresh m_pending;
  std::shared_ptr<const Game::Map::ScatterWorld> m_pending_world;
  std::shared_ptr<RefreshRun> m_running;
  std::uint64_t m_epoch{0U};
  bool m_wants_world_snapshot{false};
  // Set once a caller schedules or provides snapshots; until then a grass
  // refresh after a profile change runs synchronously as before.
  bool m_snapshots_provided{false};
};

} // namespace Render::GL
