#pragma once

#include <QMatrix4x4>

#include <memory>
#include <vector>

#include "game/map/terrain.h"
#include "linear_feature_visibility.h"
#include "render/i_render_pass.h"

namespace Render::GL {
class FogRenderer;
class Mesh;
class Renderer;
class ResourceManager;

class BridgeRenderer : public IRenderPass {
public:
  BridgeRenderer();
  ~BridgeRenderer() override;

  void configure(const std::vector<Game::Map::Bridge>& bridges,
                 float tile_size,
                 const Game::Map::TerrainHeightMap& height_map);

  void set_fog_renderer(FogRenderer* fog);

  void submit(Renderer& renderer, ResourceManager* resources) override;

private:
  void build_meshes();
  [[nodiscard]] auto segment_cull_options(float longest_segment) const
      -> Ground::LinearFeatureVisibilityOptions;

  std::vector<Game::Map::Bridge> m_bridges;
  float m_tile_size = 1.0F;
  const Game::Map::TerrainHeightMap* m_height_map = nullptr;
  FogRenderer* m_fog = nullptr;
  std::vector<std::unique_ptr<Mesh>> m_meshes;
};

} // namespace Render::GL
