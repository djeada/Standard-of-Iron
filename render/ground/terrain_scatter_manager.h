#pragma once

#include "../../game/map/map_definition.h"
#include "../i_render_pass.h"
#include "../terrain_scene_types.h"
#include <memory>
#include <mutex>
#include <vector>

namespace Render::GL {

class BiomeRenderer;
class FireCampRenderer;
class OliveRenderer;
class PineRenderer;
class PlantRenderer;
class StoneRenderer;

class TerrainScatterManager : public IRenderPass {
public:
  TerrainScatterManager();
  ~TerrainScatterManager() override;

  void configure(const Game::Map::TerrainHeightMap &height_map,
                 const Game::Map::BiomeSettings &biome_settings,
                 const std::vector<Game::Map::FireCamp> &fire_camps = {});

  void submit(Renderer &renderer, ResourceManager *resources) override;

  void clear();
  void refresh_grass();

  [[nodiscard]] bool is_gpu_ready() const;

  [[nodiscard]] auto biome() const -> BiomeRenderer *;
  [[nodiscard]] auto stone() const -> StoneRenderer *;
  [[nodiscard]] auto plant() const -> PlantRenderer *;
  [[nodiscard]] auto pine() const -> PineRenderer *;
  [[nodiscard]] auto olive() const -> OliveRenderer *;
  [[nodiscard]] auto firecamp() const -> FireCampRenderer *;
  [[nodiscard]] auto chunks() const -> std::vector<ScatterChunk>;
  [[nodiscard]] auto passes() const -> const std::vector<IRenderPass *> &;

private:
  std::unique_ptr<BiomeRenderer> m_biome;
  std::unique_ptr<StoneRenderer> m_stone;
  std::unique_ptr<PlantRenderer> m_plant;
  std::unique_ptr<PineRenderer> m_pine;
  std::unique_ptr<OliveRenderer> m_olive;
  std::unique_ptr<FireCampRenderer> m_firecamp;
  std::vector<IRenderPass *> m_passes;
  mutable std::mutex m_mutex;
};

} // namespace Render::GL
