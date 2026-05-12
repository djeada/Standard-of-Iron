

#pragma once

#include "../primitive_batch.h"

#include <QMatrix4x4>
#include <cstdint>

namespace Engine::Core {
class World;
}
namespace Game::Map {
class VisibilityService;
}

namespace Render::GL {
class Renderer;
class DrawQueue;
} // namespace Render::GL

namespace Render::Pass {

struct FrameContext {
  Render::GL::Renderer *renderer{nullptr};
  Engine::Core::World *world{nullptr};
  Render::GL::DrawQueue *queue{nullptr};

  std::uint64_t frame_counter{0};
  QMatrix4x4 view_proj;

  Render::GL::PrimitiveBatcher *primitive_batcher{nullptr};

  Game::Map::VisibilityService *visibility{nullptr};
  bool visibility_enabled{false};

  QVector3D light_direction{0.65F, 0.50F, 0.40F};
  float ambient_strength{0.30F};
};

} // namespace Render::Pass
