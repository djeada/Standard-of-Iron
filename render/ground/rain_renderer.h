#pragma once

#include "../i_render_pass.h"
#include "rain_gpu.h"
#include <QVector3D>
#include <cstdint>
#include <memory>
#include <vector>

namespace Game::Systems {
class RainManager;
}

namespace Render::GL {
class Buffer;
class Renderer;

class RainRenderer : public IRenderPass {
public:
  RainRenderer();
  ~RainRenderer() override;

  void set_enabled(bool enabled) { m_enabled = enabled; }
  [[nodiscard]] auto is_enabled() const -> bool { return m_enabled; }

  void configure(float world_width, float world_height,
                 std::uint32_t seed = 12345U,
                 Game::Map::WeatherType type = Game::Map::WeatherType::Rain);

  void set_intensity(float intensity);
  void set_weather_type(Game::Map::WeatherType type);
  void set_wind_strength(float strength);
  void set_camera_position(const QVector3D &position);

  void submit(Renderer &renderer, ResourceManager *resources) override;

  void clear();

private:
  void generate_rain_drops();
  void update_weather_params();

  bool m_enabled = false;
  float m_world_width = 100.0F;
  float m_world_height = 100.0F;
  float m_intensity = 0.0F;
  float m_target_intensity = 0.0F;
  std::uint32_t m_seed = 12345U;

  QVector3D m_camera_position{0.0F, 0.0F, 0.0F};
  float m_rain_area_radius = 50.0F;
  float m_rain_height = 30.0F;

  std::vector<RainDropInstanceGpu> m_rain_drops;
  std::unique_ptr<Buffer> m_instance_buffer;
  std::size_t m_instance_count = 0;
  RainBatchParams m_params;

  static constexpr std::size_t k_max_rain_drops = 5000;
  static constexpr std::size_t k_max_snow_drops = 3000;
  static constexpr float k_intensity_lerp_speed = 2.0F;
};

} // namespace Render::GL
