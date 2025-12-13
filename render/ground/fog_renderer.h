#pragma once

#include "../draw_queue.h"
#include "../i_render_pass.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cstdint>
#include <memory>
#include <vector>

namespace Render::GL {
class Renderer;
class ResourceManager;

class FogRenderer : public IRenderPass {
public:
  FogRenderer() = default;
  ~FogRenderer() override = default;

  void setEnabled(bool enabled) { m_enabled = enabled; }
  [[nodiscard]] auto is_enabled() const -> bool { return m_enabled; }

  void update_mask(int width, int height, float tile_size,
                  const std::vector<std::uint8_t> &cells);

  void submit(Renderer &renderer, ResourceManager *resources) override;

private:
  void build_chunks();

  using FogInstance = FogInstanceData;

  bool m_enabled = true;
  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;
  float m_half_width = 0.0F;
  float m_half_height = 0.0F;
  std::vector<std::uint8_t> m_cells;
  std::vector<FogInstance> m_instances;
};

} // namespace Render::GL
