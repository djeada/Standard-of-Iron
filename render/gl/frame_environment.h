#pragma once

#include <QVector3D>

#include "scene/environment_lighting.h"

namespace Render::GL {

class IFrameEnvironment {
public:
  IFrameEnvironment() = default;
  virtual ~IFrameEnvironment() = default;

  IFrameEnvironment(const IFrameEnvironment&) = delete;
  auto operator=(const IFrameEnvironment&) -> IFrameEnvironment& = delete;
  IFrameEnvironment(IFrameEnvironment&&) = delete;
  auto operator=(IFrameEnvironment&&) -> IFrameEnvironment& = delete;

  [[nodiscard]] virtual auto
  environment_lighting() const noexcept -> const EnvironmentLightingState& = 0;

  [[nodiscard]] virtual auto light_direction() const noexcept -> const QVector3D& = 0;

  [[nodiscard]] virtual auto ambient_strength() const noexcept -> float = 0;

  [[nodiscard]] virtual auto viewport_height() const noexcept -> int = 0;
};

} // namespace Render::GL
