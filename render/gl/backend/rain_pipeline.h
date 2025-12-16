#pragma once

#include "../shader.h"
#include "pipeline_interface.h"
#include <GL/gl.h>
#include <QMatrix4x4>
#include <QVector3D>
#include <vector>

namespace Render::GL {
class ShaderCache;
class Backend;
class Camera;
struct RainBatchParams;

namespace BackendPipelines {

struct RainDropData {
  QVector3D position;
  float speed;
  float length;
  float alpha;
};

class RainPipeline final : public IPipeline {
public:
  explicit RainPipeline(GL::Backend *backend, GL::ShaderCache *shader_cache)
      : m_backend(backend), m_shader_cache(shader_cache) {}
  ~RainPipeline() override { shutdown(); }

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

  void render(const Camera &cam, const RainBatchParams &params);

  void set_intensity(float intensity) { m_intensity = intensity; }
  void set_wind(const QVector3D &wind) { m_wind_direction = wind; }

private:
  auto create_rain_geometry() -> bool;
  void shutdown_geometry();
  void generate_rain_drops();

  GL::Backend *m_backend = nullptr;
  GL::ShaderCache *m_shader_cache = nullptr;
  GL::Shader *m_rain_shader = nullptr;

  GLuint m_vao = 0;
  GLuint m_vertex_buffer = 0;
  GLuint m_index_buffer = 0;
  GLsizei m_index_count = 0;

  float m_intensity = 0.0F;
  QVector3D m_wind_direction{0.1F, 0.0F, 0.0F};

  std::vector<RainDropData> m_rain_drops;
  static constexpr std::size_t k_max_drops = 3000;
  static constexpr float k_drop_speed = 20.0F;
  static constexpr float k_drop_length = 1.2F;
  static constexpr float k_area_radius = 50.0F;
  static constexpr float k_area_height = 30.0F;

  struct RainUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle intensity{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle camera_pos{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle rain_color{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle wind{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle weather_type{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle wind_strength{GL::Shader::InvalidUniform};
  };

  RainUniforms m_uniforms;
};

} // namespace BackendPipelines
} // namespace Render::GL
