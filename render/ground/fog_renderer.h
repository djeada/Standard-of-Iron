#pragma once

#include <QMatrix4x4>
#include <QVector3D>
#include <cstdint>
#include <memory>
#include <vector>

namespace Render {
namespace GL {
class Renderer;
class ResourceManager;
class Mesh;
class Texture;

class FogRenderer {
public:
  FogRenderer() = default;
  ~FogRenderer() = default;

  void setEnabled(bool enabled) { m_enabled = enabled; }
  bool isEnabled() const { return m_enabled; }

  void updateMask(int width, int height, float tileSize,
                  const std::vector<std::uint8_t> &cells);

  void submit(Renderer &renderer, ResourceManager &resources);

private:
  void buildChunks();

  struct FogChunk {
    std::unique_ptr<Mesh> mesh;
    QVector3D color{0.05f, 0.05f, 0.05f};
    float alpha = 0.45f;
  };

  bool m_enabled = true;
  int m_width = 0;
  int m_height = 0;
  float m_tileSize = 1.0f;
  float m_halfWidth = 0.0f;
  float m_halfHeight = 0.0f;
  std::vector<std::uint8_t> m_cells;
  std::vector<FogChunk> m_chunks;
};

} // namespace GL
} // namespace Render
