#pragma once

#include <QVector3D>

#include <memory>
#include <mutex>
#include <vector>

#include "../../game/map/map_definition.h"
#include "../i_render_pass.h"
#include "../terrain_scene_types.h"

namespace Render::GL {

class BiomeRenderer;
class BoulderRenderer;
class DeadTreeRenderer;
class FireCampRenderer;
class IronOreRenderer;
class MagicShrineRenderer;
class OliveRenderer;
class PineRenderer;
class PlantRenderer;
class RuinsRenderer;
class StoneRenderer;
class SupplyCartRenderer;
class TentRenderer;
class WeaponRackRenderer;

class TerrainScatterManager : public IRenderPass {
public:
  TerrainScatterManager();
  ~TerrainScatterManager() override;

  void configure(const Game::Map::TerrainHeightMap& height_map,
                 const Game::Map::BiomeSettings& biome_settings,
                 const std::vector<Game::Map::WorldProp>& world_props = {},
                 bool use_world_props_exclusively = false);

  void set_light_direction(const QVector3D& dir);

  void submit(Renderer& renderer, ResourceManager* resources) override;

  void clear();
  void refresh_grass();

  [[nodiscard]] bool is_gpu_ready() const;

  [[nodiscard]] auto biome() const -> BiomeRenderer*;
  [[nodiscard]] auto stone() const -> StoneRenderer*;
  [[nodiscard]] auto plant() const -> PlantRenderer*;
  [[nodiscard]] auto pine() const -> PineRenderer*;
  [[nodiscard]] auto olive() const -> OliveRenderer*;
  [[nodiscard]] auto firecamp() const -> FireCampRenderer*;
  [[nodiscard]] auto tent() const -> TentRenderer*;
  [[nodiscard]] auto supply_cart() const -> SupplyCartRenderer*;
  [[nodiscard]] auto weapon_rack() const -> WeaponRackRenderer*;
  [[nodiscard]] auto ruins() const -> RuinsRenderer*;
  [[nodiscard]] auto dead_tree() const -> DeadTreeRenderer*;
  [[nodiscard]] auto boulder() const -> BoulderRenderer*;
  [[nodiscard]] auto iron_ore() const -> IronOreRenderer*;
  [[nodiscard]] auto magic_shrine() const -> MagicShrineRenderer*;
  [[nodiscard]] auto chunks() const -> std::vector<ScatterChunk>;
  [[nodiscard]] auto last_sync_stats() const -> Render::Ground::Scatter::SyncStats;
  [[nodiscard]] auto passes() const -> const std::vector<IRenderPass*>&;

private:
  std::unique_ptr<BiomeRenderer> m_biome;
  std::unique_ptr<StoneRenderer> m_stone;
  std::unique_ptr<PlantRenderer> m_plant;
  std::unique_ptr<PineRenderer> m_pine;
  std::unique_ptr<OliveRenderer> m_olive;
  std::unique_ptr<FireCampRenderer> m_firecamp;
  std::unique_ptr<TentRenderer> m_tent;
  std::unique_ptr<SupplyCartRenderer> m_supply_cart;
  std::unique_ptr<WeaponRackRenderer> m_weapon_rack;
  std::unique_ptr<RuinsRenderer> m_ruins;
  std::unique_ptr<DeadTreeRenderer> m_dead_tree;
  std::unique_ptr<BoulderRenderer> m_boulder;
  std::unique_ptr<IronOreRenderer> m_iron_ore;
  std::unique_ptr<MagicShrineRenderer> m_magic_shrine;
  std::vector<IRenderPass*> m_passes;
  mutable std::mutex m_mutex;
};

} // namespace Render::GL
