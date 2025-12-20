#pragma once

#include "../shader.h"
#include "../shader_cache.h"
#include "pipeline_interface.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cstddef>
#include <memory>
#include <vector>

namespace Render::GL {
class Backend;
class Mesh;
class Texture;
} // namespace Render::GL

namespace Render::GL::BackendPipelines {

/// Per-instance data for mesh instancing (model matrix + color/alpha).
/// Layout matches shader attribute requirements.
struct MeshInstanceGpu {
  float model_col0[4]{1, 0, 0, 0}; // Model matrix column 0
  float model_col1[4]{0, 1, 0, 0}; // Model matrix column 1
  float model_col2[4]{0, 0, 1, 0}; // Model matrix column 2
  float model_col3[4]{0, 0, 0, 1}; // Model matrix column 3
  float color[3]{1, 1, 1};         // RGB color
  float alpha{1.0F};               // Alpha value
  int material_id{0};              // Material ID
  float padding[3]{0, 0, 0};       // Padding for alignment
};

/// Mesh instancing pipeline for batching repeated mesh+shader+texture draws.
/// Accumulates instance data for consecutive identical draws and issues
/// a single glDrawElementsInstanced call per batch.
class MeshInstancingPipeline final : public IPipeline {
public:
  explicit MeshInstancingPipeline(GL::Backend *backend,
                                  GL::ShaderCache *shader_cache);
  ~MeshInstancingPipeline() override;

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

  /// Begin a new frame - clears accumulated instances.
  void begin_frame();

  /// Check if the given mesh/shader/texture combo matches current batch.
  [[nodiscard]] auto can_batch(Mesh *mesh, Shader *shader,
                               Texture *texture) const -> bool;

  /// Accumulate an instance into the current batch.
  /// @param model Model matrix for this instance.
  /// @param color RGB color for this instance.
  /// @param alpha Alpha value for this instance.
  /// @param material_id Material ID for this instance.
  void accumulate(const QMatrix4x4 &model, const QVector3D &color, float alpha,
                  int material_id);

  /// Start a new batch with the given mesh/shader/texture combo.
  void begin_batch(Mesh *mesh, Shader *shader, Texture *texture);

  /// Flush accumulated instances using instanced rendering.
  /// @param view_proj View-projection matrix for the batch.
  void flush(const QMatrix4x4 &view_proj);

  /// Returns the number of instances in the current batch.
  [[nodiscard]] auto instance_count() const -> std::size_t;

  /// Returns true if there are pending instances to flush.
  [[nodiscard]] auto has_pending() const -> bool;

private:
  void upload_instances();
  void setup_instance_attributes();

  GL::Backend *m_backend{nullptr};
  GL::ShaderCache *m_shaderCache{nullptr};
  bool m_initialized{false};

  // Current batch state
  Mesh *m_currentMesh{nullptr};
  Shader *m_currentShader{nullptr};
  Texture *m_currentTexture{nullptr};

  // Instance data accumulation
  std::vector<MeshInstanceGpu> m_instances;
  std::size_t m_instanceCapacity{0};

  // OpenGL resources for instance buffer
  GLuint m_instanceBuffer{0};
  GLuint m_instanceVao{0};

  // Uniform locations for instanced rendering
  struct Uniforms {
    Shader::UniformHandle view_proj{Shader::InvalidUniform};
    Shader::UniformHandle texture{Shader::InvalidUniform};
    Shader::UniformHandle useTexture{Shader::InvalidUniform};
  };
  Uniforms m_uniforms;
};

} // namespace Render::GL::BackendPipelines
