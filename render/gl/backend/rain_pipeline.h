#pragma once

#include <QtGui/qopengl.h>

#include <cstddef>
#include <vector>

#include "pipeline_interface.h"
#include "render/gl/frame_environment.h"
#include "render/gl/shader.h"

namespace Render::GL {
class ShaderCache;
class Camera;
struct RainBatchParams;

namespace BackendPipelines {

struct WeatherParticleGpu {

  float seed[4];

  float props[4];
};

class RainPipeline final : public IPipeline {
public:
  explicit RainPipeline(const GL::IFrameEnvironment* frame_environment,
                        GL::ShaderCache* shader_cache)
      : m_frame_environment(frame_environment)
      , m_shader_cache(shader_cache) {}
  ~RainPipeline() override { shutdown(); }

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

  void render(const Camera& cam, const RainBatchParams& params);

  static constexpr std::size_t k_max_particles = 16384;

private:
  auto create_geometry() -> bool;
  void shutdown_geometry();
  void generate_particles();

  const GL::IFrameEnvironment* m_frame_environment = nullptr;
  GL::ShaderCache* m_shader_cache = nullptr;
  GL::Shader* m_rain_shader = nullptr;

  GLuint m_vao = 0;
  GLuint m_quad_buffer = 0;
  GLuint m_index_buffer = 0;
  GLuint m_instance_buffer = 0;

  std::vector<WeatherParticleGpu> m_particles;

  struct RainUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle camera_pos{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle camera_right{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle camera_up{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle intensity{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle density{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle rank_step{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle weather_type{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle wind{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle wind_strength{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle fall_speed{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle streak_half_length{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle particle_half_size{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle field{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle pixel_scale{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle speed_range{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle size_range{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle alpha_range{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle rain_color{GL::Shader::InvalidUniform};
  };

  RainUniforms m_uniforms;
};

} // namespace BackendPipelines
} // namespace Render::GL
