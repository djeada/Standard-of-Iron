#pragma once

#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>

#include <cstdint>

#include "render/frame/primitive_batch.h"

namespace Engine::Core {
class World;
}
namespace Game::Map {
class VisibilityService;
}

namespace Render::GL {
class Camera;
class DrawQueue;
class Renderer;
class Texture;
} // namespace Render::GL

namespace Render::Pass {

struct SurfaceResources {

  struct GroundCover {
    Render::GL::Texture* texture{nullptr};
    QVector2D uv_scale{0.0F, 0.0F};
    QVector2D uv_offset{0.5F, 0.5F};
  };
  GroundCover ground_cover;
};

struct FrameContext {
  Render::GL::Renderer* renderer{nullptr};
  Engine::Core::World* world{nullptr};
  Render::GL::DrawQueue* queue{nullptr};

  std::uint64_t frame_counter{0};
  QMatrix4x4 view_proj;

  Render::GL::PrimitiveBatcher* primitive_batcher{nullptr};

  const Game::Map::VisibilityService* visibility{nullptr};
  bool visibility_enabled{false};

  QVector3D light_direction{0.65F, 0.50F, 0.40F};
  float ambient_strength{0.30F};

  const Render::GL::Camera* camera{nullptr};
  QMatrix4x4 view;
  QMatrix4x4 projection;
  int viewport_width{0};
  int viewport_height{0};
  float animation_time{0.0F};
  float banner_wind_strength{0.0F};

  SurfaceResources surface;
};

} // namespace Render::Pass
