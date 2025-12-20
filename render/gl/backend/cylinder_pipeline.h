#pragma once

#include "../persistent_buffer.h"
#include "../shader_cache.h"
#include "pipeline_interface.h"
#include <QVector3D>
#include <memory>
#include <vector>

namespace Render::GL::BackendPipelines {

class CylinderPipeline : public IPipeline {
public:
  explicit CylinderPipeline(GL::ShaderCache *shader_cache);
  ~CylinderPipeline() override;

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override {
    return m_initialized;
  }

  void begin_frame();

  void upload_cylinder_instances(std::size_t count);
  void draw_cylinders(std::size_t count);

  void upload_fog_instances(std::size_t count);
  void draw_fog(std::size_t count);

  [[nodiscard]] auto cylinderShader() const -> GL::Shader * {
    return m_cylinderShader;
  }
  [[nodiscard]] auto fogShader() const -> GL::Shader * { return m_fogShader; }

  struct CylinderUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
  };

  struct FogUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
  };

  struct CylinderInstanceGpu {
    QVector3D start;
    float radius{0.0F};
    QVector3D end;
    float alpha{1.0F};
    QVector3D color;
    float padding{0.0F};
  };

  struct FogInstanceGpu {
    QVector3D center;
    float size{1.0F};
    QVector3D color;
    float alpha{1.0F};
  };

  CylinderUniforms m_cylinderUniforms;
  FogUniforms m_fogUniforms;
  std::vector<CylinderInstanceGpu> m_cylinderScratch;
  std::vector<FogInstanceGpu> m_fogScratch;

private:
  void initialize_cylinder_pipeline();
  void shutdown_cylinder_pipeline();
  void initialize_fog_pipeline();
  void shutdown_fog_pipeline();

  GL::ShaderCache *m_shaderCache;
  bool m_initialized{false};
  bool m_usePersistentBuffers{false};

  GL::Shader *m_cylinderShader{nullptr};
  GLuint m_cylinderVao{0};
  GLuint m_cylinderVertexBuffer{0};
  GLuint m_cylinderIndexBuffer{0};
  GLuint m_cylinderInstanceBuffer{0};
  GLsizei m_cylinderIndexCount{0};
  std::size_t m_cylinderInstanceCapacity{0};
  GL::PersistentRingBuffer<CylinderInstanceGpu> m_cylinderPersistentBuffer;

  GL::Shader *m_fogShader{nullptr};
  GLuint m_fogVao{0};
  GLuint m_fogVertexBuffer{0};
  GLuint m_fogIndexBuffer{0};
  GLuint m_fogInstanceBuffer{0};
  GLsizei m_fogIndexCount{0};
  std::size_t m_fogInstanceCapacity{0};
  GL::PersistentRingBuffer<FogInstanceGpu> m_fogPersistentBuffer;
};

} // namespace Render::GL::BackendPipelines
