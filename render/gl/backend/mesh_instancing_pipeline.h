#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "instance_draw_guard.h"
#include "pipeline_interface.h"
#include "render/gl/shader.h"
#include "render/gl/shader_cache.h"

namespace Render::GL {
class Mesh;
struct MergedBuildingMesh;
} // namespace Render::GL

namespace Render::GL::BackendPipelines {

using Render::GL::MergedBuildingMesh;

struct MeshInstanceGpu {
  float model_col0[4]{1, 0, 0, 0};
  float model_col1[4]{0, 1, 0, 0};
  float model_col2[4]{0, 0, 1, 0};
  float color_alpha[4]{1, 1, 1, 1};
};

class MeshInstancingPipeline final : public IPipeline {
public:
  explicit MeshInstancingPipeline(GL::ShaderCache* shader_cache);
  ~MeshInstancingPipeline() override;

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

  void begin_frame();

  void accumulate(const QMatrix4x4& model, const QVector3D& color, float alpha);

  void begin_batch(Mesh* mesh);

  void flush();

  auto upload(const void* data, std::size_t bytes, std::size_t& byte_offset) -> bool;

  void draw_merged(const MergedBuildingMesh& mesh,
                   std::size_t instance_byte_offset,
                   std::size_t instance_count,
                   std::uint32_t first_index,
                   std::uint32_t index_count);

private:
  struct MergedBuffers {
    GLuint vao{0};
    GLuint vertices{0};
    GLuint indices{0};
  };

  auto bind_mesh(Mesh* mesh) -> bool;
  void point_instance_attributes(std::size_t byte_offset);
  auto merged_vao(const MergedBuildingMesh& mesh) -> GLuint;

  bool m_initialized{false};

  Mesh* m_current_mesh{nullptr};

  std::vector<MeshInstanceGpu> m_instances;
  std::size_t m_instance_capacity{0};

  GLuint m_instance_buffer{0};
  std::size_t m_ring_capacity_bytes{0};
  std::size_t m_ring_offset_bytes{0};

  std::unordered_map<std::uint64_t, MergedBuffers> m_merged;

  InstanceDrawGuard m_draw_guard{"MeshInstancingPipeline"};
};

} // namespace Render::GL::BackendPipelines
