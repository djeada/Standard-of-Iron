#pragma once

#include "../draw_queue.h"
#include "../i_render_pass.h"
#include <vector>

namespace Render::GL {
class Renderer;
class ResourceManager;

class MapBoundaryFogRenderer : public IRenderPass {
public:
  MapBoundaryFogRenderer() = default;
  ~MapBoundaryFogRenderer() override = default;

  void configure(int width, int height, float tile_size);

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
