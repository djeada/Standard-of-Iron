#pragma once

#include "../draw_queue.h"
#include "../i_render_pass.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cstdint>
#include <memory>
#include <vector>

namespace Render {
namespace GL {
class Renderer;
class ResourceManager;

class FogRenderer : public IRenderPass {
public:
  FogRenderer() = default;
  ~FogRenderer() = default;

  void setEnabled(bool enabled) { m_enabled = enabled; }
  bool isEnabled() const { return m_enabled; }

  void updateMask(int width, int height, float tileSize,
                  const std::vector<std::uint8_t> &cells);

  void submit(Renderer &renderer, ResourceManager *resources) override;

private:
  void buildChunks();

  using FogInstance = FogInstanceData;

  bool m_enabled = true;
  int m_width = 0;
  int m_height = 0;
  float m_tileSize = 1.0f;
  float m_halfWidth = 0.0f;
  float m_halfHeight = 0.0f;
  std::vector<std::uint8_t> m_cells;
  std::vector<FogInstance> m_instances;
};

} 
} 
