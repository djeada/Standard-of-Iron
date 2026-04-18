// Stage 10 — per-frame context passed to each FramePass.
//
// Holds the pointers and scratch that the phases of render_world used to
// share via stack locals. Any member that is expensive to re-derive lives
// here so later passes don't recompute it.
//
// Keeping this POD lets tests construct a minimal FrameContext on the
// stack without pulling a real ECS world or GL context. Passes are pure
// logic that reads/writes FrameContext fields.

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

  // Shared scratch. Passes that come later in the runner may read these.
  Render::GL::PrimitiveBatcher *primitive_batcher{nullptr};

  Game::Map::VisibilityService *visibility{nullptr};
  bool visibility_enabled{false};
};

} // namespace Render::Pass
