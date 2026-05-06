#pragma once

#include "../draw_queue.h"
#include "../i_render_pass.h"
#include <vector>

namespace Render::GL {
class Renderer;
class ResourceManager;

// Renders a lightweight semi-transparent fog band around the map boundary to
// soften the visual transition into out-of-bounds space. Fog instances are
// pre-built at configure() time and reused every frame with zero CPU overhead.
class MapBoundaryFogRenderer : public IRenderPass {
public:
  MapBoundaryFogRenderer() = default;
  ~MapBoundaryFogRenderer() override = default;

  // Reconfigures the boundary fog for a map of the given dimensions.
  // Rebuilds the instance list; call once per map load.
  void configure(int width, int height, float tile_size);

  // Returns the number of pre-built fog instances (useful for tests/profiling).
  [[nodiscard]] auto instance_count() const -> std::size_t {
    return m_instances.size();
  }

  void submit(Renderer &renderer, ResourceManager *resources) override;

private:
  void build_instances();

  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;
  std::vector<FogInstanceData> m_instances;
};

} // namespace Render::GL
