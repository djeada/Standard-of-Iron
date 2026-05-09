#pragma once

#include "../gl/mesh.h"
#include "../i_render_pass.h"
#include <QVector2D>
#include <QVector3D>
#include <memory>
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
    return m_cards.size();
  }

  [[nodiscard]] auto mountain_triangle_count() const -> std::size_t {
    return m_mountain_indices.size() / 3U;
  }

  void submit(Renderer &renderer, ResourceManager *resources) override;

private:
  struct BoundaryCard {
    QVector3D center{0.0F, 0.0F, 0.0F};
    QVector2D size{1.0F, 1.0F};
    float yaw_degrees{0.0F};
    QVector3D color{0.33F, 0.35F, 0.33F};
    float alpha{1.0F};
  };

  void build_cards();
  void build_mountains();
  void ensure_mountain_mesh();

  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;
  std::vector<BoundaryCard> m_cards;
  std::vector<Vertex> m_mountain_vertices;
  std::vector<unsigned int> m_mountain_indices;
  std::unique_ptr<Mesh> m_mountain_mesh;
};

} // namespace Render::GL
