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

struct MeshInstanceGpu {
  float model_col0[4]{1, 0, 0, 0};
  float model_col1[4]{0, 1, 0, 0};
  float model_col2[4]{0, 0, 1, 0};
  float color_alpha[4]{1, 1, 1, 1};
};

class MeshInstancingPipeline final : public IPipeline {
public:
  explicit MeshInstancingPipeline(GL::Backend *backend,
                                  GL::ShaderCache *shader_cache);
  ~MeshInstancingPipeline() override;

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

  void begin_frame();

  [[nodiscard]] auto can_batch(Mesh *mesh, Shader *shader,
                               Texture *texture) const -> bool;

  void accumulate(const QMatrix4x4 &model, const QVector3D &color, float alpha,
                  int material_id = 0);

  void begin_batch(Mesh *mesh, Shader *shader, Texture *texture);

  void flush();

  [[nodiscard]] auto instance_count() const -> std::size_t;

  [[nodiscard]] auto has_pending() const -> bool;

private:
  void setup_instance_attributes();

  GL::Backend *m_backend{nullptr};
  GL::ShaderCache *m_shaderCache{nullptr};
  bool m_initialized{false};

  Mesh *m_currentMesh{nullptr};
  Shader *m_currentShader{nullptr};
  Texture *m_currentTexture{nullptr};

  std::vector<MeshInstanceGpu> m_instances;
  std::size_t m_instanceCapacity{0};

  GLuint m_instanceBuffer{0};
};

} // namespace Render::GL::BackendPipelines
