

#pragma once

#include "gl/buffer.h"

#include <QOpenGLFunctions_3_3_Core>
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace Render::GL {

struct RiggedVertex {
  std::array<float, 3> position_bone_local{};
  std::array<float, 3> normal_bone_local{};
  std::array<float, 2> tex_coord{};
  std::array<std::uint8_t, 4> bone_indices{};
  std::array<float, 4> bone_weights{};
  std::uint8_t color_role{0};
  std::array<std::uint8_t, 3> padding{};
};

class RiggedMesh : protected QOpenGLFunctions_3_3_Core {
public:
  RiggedMesh(std::vector<RiggedVertex> vertices,
             std::vector<std::uint32_t> indices);
  ~RiggedMesh() override;

  auto bind_vao() -> bool;
  void unbind_vao();

  void draw();

  [[nodiscard]] auto
  get_vertices() const noexcept -> const std::vector<RiggedVertex> & {
    return m_vertices;
  }
  [[nodiscard]] auto
  get_indices() const noexcept -> const std::vector<std::uint32_t> & {
    return m_indices;
  }

  [[nodiscard]] auto vertex_count() const noexcept -> std::size_t {
    return m_vertices.size();
  }
  [[nodiscard]] auto index_count() const noexcept -> std::size_t {
    return m_indices.size();
  }

  auto ensure_gl_buffers() -> bool;
  [[nodiscard]] auto vertex_buffer() noexcept -> Buffer * {
    return m_vbo.get();
  }
  [[nodiscard]] auto index_buffer() noexcept -> Buffer * { return m_ebo.get(); }

private:
  std::vector<RiggedVertex> m_vertices;
  std::vector<std::uint32_t> m_indices;

  std::unique_ptr<VertexArray> m_vao;
  std::unique_ptr<Buffer> m_vbo;
  std::unique_ptr<Buffer> m_ebo;

  void setup_buffers();
};

} // namespace Render::GL
