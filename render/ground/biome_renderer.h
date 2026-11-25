#pragma once

#include <QVector3D>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "game/map/biome_settings.h"
#include "game/map/scatter/spawn_validator.h"
#include "game/map/terrain.h"
#include "render/frame/decoration_gpu.h"
#include "render/ground/scatter_renderer_base.h"

namespace Render::Ground::Scatter {
struct GrassScatter;
}

namespace Render::GL {
class FrameSubmitter;

class BiomeRenderer : public ScatterRendererBase<GrassInstanceGpu, GrassBatchParams> {
public:
  BiomeRenderer();
  ~BiomeRenderer() override;

  void configure(const Game::Map::TerrainHeightMap& height_map,
                 const Game::Map::BiomeSettings& biome_settings);

  [[nodiscard]] auto plan_grass(float density,
                                std::shared_ptr<const Game::Map::ScatterWorld> world)
      -> Render::Ground::Scatter::ScatterJob;

  void set_light_direction(const QVector3D& dir) override;

  void submit(FrameSubmitter& renderer, const DrawResources* resources) override;

  void refresh_grass();
  [[nodiscard]] auto grass_matches_graphics_profile() const noexcept -> bool;

private:
  void adopt_grass(Ground::Scatter::GrassScatter grass);
  void generate_grass_instances();

  float m_generated_grass_density{-1.0F};
  float m_typical_blade_height = 0.0F;
};

} // namespace Render::GL
