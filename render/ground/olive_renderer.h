#pragma once

#include <QVector3D>

#include <cstdint>
#include <memory>
#include <vector>

#include "game/map/map_definition.h"
#include "game/map/terrain.h"
#include "render/decoration_gpu.h"
#include "scatter_renderer_base.h"

namespace Render::GL {
class Buffer;
class Renderer;

class OliveRenderer : public ScatterRendererBase<TreeInstanceGpu, FoliageBatchParams> {
public:
  OliveRenderer();
  ~OliveRenderer() override;

  void configure(const Game::Map::TerrainHeightMap& height_map,
                 const Game::Map::BiomeSettings& biome_settings,
                 const std::vector<Game::Map::WorldProp>& scatter_seed_world_props = {},
                 const std::vector<Game::Map::WorldProp>& runtime_world_props = {},
                 bool use_world_props_exclusively = false);

  void refresh_world_props(const std::vector<Game::Map::WorldProp>& runtime_world_props,
                           bool use_world_props_exclusively);

  void set_light_direction(const QVector3D& dir) override;

  void submit(Renderer& renderer, ResourceManager* resources) override;

  [[nodiscard]] auto instances_for_test() const -> const std::vector<TreeInstanceGpu>& {
    return m_state.instances;
  }

private:
  void rebuild_olive_instances();
  void append_world_prop_olives();
  void generate_procedural_olives(std::vector<TreeInstanceGpu>& out) const;
};

} // namespace Render::GL
