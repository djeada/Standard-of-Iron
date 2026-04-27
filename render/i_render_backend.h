#pragma once

#include <cstdint>

class QString;

namespace Render {

struct FrameBudgetConfig;
class FrameTimeTracker;

enum class ShaderQuality : std::uint8_t {
  Full = 0,
  Reduced = 1,
  Minimal = 2,
  None = 3
};

} // namespace Render

namespace Render::GL {

class DrawQueue;
class Camera;
class ResourceManager;
class Shader;

class IRenderBackend {
public:
  virtual ~IRenderBackend() = default;

  virtual void initialize() = 0;
  virtual void begin_frame() = 0;
  virtual void execute(const DrawQueue &queue, const Camera &cam) = 0;

  virtual void set_viewport(int w, int h) = 0;
  virtual void set_clear_color(float r, float g, float b, float a) = 0;
  virtual void set_animation_time(float time) noexcept = 0;
  virtual void set_frame_budget(const Render::FrameBudgetConfig &config) = 0;

  [[nodiscard]] virtual auto resources() const -> ResourceManager * = 0;
  [[nodiscard]] virtual auto shader(const QString &name) const -> Shader * = 0;
  [[nodiscard]] virtual auto supports_shaders() const -> bool = 0;
  [[nodiscard]] virtual auto shader_quality() const -> ShaderQuality = 0;
  [[nodiscard]] virtual auto
  frame_tracker() const -> const Render::FrameTimeTracker * = 0;
};

} // namespace Render::GL
