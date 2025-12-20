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
/// Layout matches the primitive_instanced.vert shader attribute pattern:
/// - i_modelCol0 (location 3): xyz = col0, w = translation.x
/// - i_modelCol1 (location 4): xyz = col1, w = translation.y
/// - i_modelCol2 (location 5): xyz = col2, w = translation.z
/// - i_colorAlpha (location 6): rgb = color, a = alpha
struct MeshInstanceGpu {
  float model_col0[4]{1, 0, 0, 0}; // xyz = mat col 0, w = translation.x
  float model_col1[4]{0, 1, 0, 0}; // xyz = mat col 1, w = translation.y
  float model_col2[4]{0, 0, 1, 0}; // xyz = mat col 2, w = translation.z
  float color_alpha[4]{1, 1, 1, 1}; // rgb = color, a = alpha
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
  /// @param material_id Material ID for this instance (currently unused).
  void accumulate(const QMatrix4x4 &model, const QVector3D &color, float alpha,
                  int material_id = 0);

  /// Start a new batch with the given mesh/shader/texture combo.
  void begin_batch(Mesh *mesh, Shader *shader, Texture *texture);

  /// Flush accumulated instances using instanced rendering.
  /// The current shader must already be bound with view_proj uniform set.
  void flush();

  /// Returns the number of instances in the current batch.
  [[nodiscard]] auto instance_count() const -> std::size_t;

  /// Returns true if there are pending instances to flush.
  [[nodiscard]] auto has_pending() const -> bool;

private:
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
};

} // namespace Render::GL::BackendPipelines
