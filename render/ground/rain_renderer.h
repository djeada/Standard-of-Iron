#pragma once

#include <QVector3D>

#include <cstdint>

#include "../i_render_pass.h"
#include "../rain_gpu.h"

namespace Game::Systems {
class RainManager;
}

namespace Render::GL {
class Renderer;

class RainRenderer : public IRenderPass {
public:
  RainRenderer();
  ~RainRenderer() override;

  void set_enabled(bool enabled) { m_enabled = enabled; }
  [[nodiscard]] auto is_enabled() const -> bool { return m_enabled; }

  void configure(float world_width,
                 float world_height,
                 std::uint32_t seed = 12345U,
                 Game::Map::WeatherType type = Game::Map::WeatherType::Rain);

  void set_intensity(float intensity);
  void set_weather_type(Game::Map::WeatherType type);
  void set_wind_strength(float strength);
  void set_wind_direction_deg(float degrees);
  void set_camera_position(const QVector3D& position);

  void submit(Renderer& renderer, ResourceManager* resources) override;

  void clear();

  [[nodiscard]] auto intensity() const noexcept -> float { return m_intensity; }
  [[nodiscard]] auto params() const noexcept -> const RainBatchParams& {
    return m_params;
  }

private:
  void update_weather_params();

  bool m_enabled = false;
  float m_world_width = 100.0F;
  float m_world_height = 100.0F;
  float m_intensity = 0.0F;
  float m_target_intensity = 0.0F;
  std::uint32_t m_seed = 12345U;

  QVector3D m_camera_position{0.0F, 0.0F, 0.0F};

  RainBatchParams m_params;

  static constexpr float k_intensity_lerp_speed = 2.0F;
};

} // namespace Render::GL
