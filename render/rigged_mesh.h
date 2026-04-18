// Stage 15.5a — RiggedMesh + RiggedVertex.
//
// GPU-skinning-ready vertex layout and mesh wrapper. Mirrors the shape of
// `Render::GL::Mesh` but carries per-vertex bone indices + weights and
// stores positions/normals in each vertex's anchor-bone local frame.
//
// This stage (15.5a) only introduces the infrastructure: there is no
// pipeline/shader consuming it yet. The GL upload is lazy (on first
// draw/bind), matching the existing `Mesh` contract so headless tests
// can exercise the CPU-side bake without a live GL context.
//
// TODO(stage-15.5b): once bind-pose inverse matrices are plumbed in, the
// "bone-local" frame will stop being a no-op at identity pose and start
// carrying inverse_bind * primitive_world * vertex_local.

#pragma once

#include "gl/buffer.h"

#include <QOpenGLFunctions_3_3_Core>
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace Render::GL {

// Rigged vertex. Positions/normals are expressed in the anchor bone's
// local frame at bind time (identity-pose for stage 15.5a, so
// numerically this is just primitive_world * vertex_local). `bone_indices`
// holds up to four bone references (primary + up to three secondaries);
// `bone_weights` must sum to 1.0 and has weight[i] paired with
// bone_indices[i]. Unused slots should have weight == 0 and index == 0.
struct RiggedVertex {
  std::array<float, 3> position_bone_local{};
  std::array<float, 3> normal_bone_local{};
  std::array<float, 2> tex_coord{};
  std::array<std::uint8_t, 4> bone_indices{};
  std::array<float, 4> bone_weights{};
};

class RiggedMesh : protected QOpenGLFunctions_3_3_Core {
public:
  RiggedMesh(std::vector<RiggedVertex> vertices,
             std::vector<std::uint32_t> indices);
  ~RiggedMesh() override;

  // Bind the VAO (lazy-initialised on first call with a live GL
  // context). Returns true if the VAO is bound; false when called
  // without a current GL context (e.g. headless tests).
  auto bind_vao() -> bool;
  void unbind_vao();

  // Issue a glDrawElements for the full index buffer. No-ops if no
  // GL context is available.
  void draw();

  [[nodiscard]] auto get_vertices() const noexcept
      -> const std::vector<RiggedVertex> & {
    return m_vertices;
  }
  [[nodiscard]] auto get_indices() const noexcept
      -> const std::vector<std::uint32_t> & {
    return m_indices;
  }

  [[nodiscard]] auto vertex_count() const noexcept -> std::size_t {
    return m_vertices.size();
  }
  [[nodiscard]] auto index_count() const noexcept -> std::size_t {
    return m_indices.size();
  }

  // Stage 16.4 — accessors for the instanced pipeline. Lazily realises
  // the underlying GL buffers (no-op without a current GL context) so
  // an instanced draw can build its own VAO referencing this mesh's
  // vertex/index storage. Returns `nullptr` when no GL context is
  // current AND the buffers have never been built.
  auto ensure_gl_buffers() -> bool;
  [[nodiscard]] auto vertex_buffer() noexcept -> Buffer * { return m_vbo.get(); }
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
