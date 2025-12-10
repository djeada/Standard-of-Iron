#pragma once

#include "../../primitive_batch.h"
#include "../persistent_buffer.h"
#include "../shader_cache.h"
#include "pipeline_interface.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <memory>
#include <vector>

namespace Render::GL::BackendPipelines {

class PrimitiveBatchPipeline : public IPipeline {
public:
  explicit PrimitiveBatchPipeline(GL::ShaderCache *shaderCache);
  ~PrimitiveBatchPipeline() override;

  auto initialize() -> bool override;
  void shutdown() override;
  void cacheUniforms() override;
  [[nodiscard]] auto isInitialized() const -> bool override {
    return m_initialized;
  }

  void beginFrame();

  void uploadSphereInstances(const GL::PrimitiveInstanceGpu *data,
                             std::size_t count);
  void uploadCylinderInstances(const GL::PrimitiveInstanceGpu *data,
                               std::size_t count);
  void uploadConeInstances(const GL::PrimitiveInstanceGpu *data,
                           std::size_t count);

  void drawSpheres(std::size_t count, const QMatrix4x4 &viewProj);
  void drawCylinders(std::size_t count, const QMatrix4x4 &viewProj);
  void drawCones(std::size_t count, const QMatrix4x4 &viewProj);

  [[nodiscard]] auto shader() const -> GL::Shader * { return m_shader; }

  struct Uniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_dir{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle ambient_strength{GL::Shader::InvalidUniform};
  };

  Uniforms m_uniforms;

private:
  void initializeSphereVao();
  void initializeCylinderVao();
  void initializeConeVao();
  void shutdownVaos();

  void setupInstanceAttributes(GLuint vao, GLuint instanceBuffer);

  GL::ShaderCache *m_shaderCache;
  bool m_initialized{false};

  GL::Shader *m_shader{nullptr};

  GLuint m_sphereVao{0};
  GLuint m_sphereVertexBuffer{0};
  GLuint m_sphereIndexBuffer{0};
  GLuint m_sphereInstanceBuffer{0};
  GLsizei m_sphereIndexCount{0};
  std::size_t m_sphereInstanceCapacity{0};

  GLuint m_cylinderVao{0};
  GLuint m_cylinderVertexBuffer{0};
  GLuint m_cylinderIndexBuffer{0};
  GLuint m_cylinderInstanceBuffer{0};
  GLsizei m_cylinderIndexCount{0};
  std::size_t m_cylinderInstanceCapacity{0};

  GLuint m_coneVao{0};
  GLuint m_coneVertexBuffer{0};
  GLuint m_coneIndexBuffer{0};
  GLuint m_coneInstanceBuffer{0};
  GLsizei m_coneIndexCount{0};
  std::size_t m_coneInstanceCapacity{0};

  static constexpr std::size_t kDefaultInstanceCapacity = 4096;
  static constexpr float kGrowthFactor = 1.5F;
};

} // namespace Render::GL::BackendPipelines
