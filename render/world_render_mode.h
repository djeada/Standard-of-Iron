#pragma once

namespace Render::GL {

// Which game the frame is drawing: the top-down strategy view, or the
// behind-the-shoulder RPG view. The two differ in what may be culled and in
// which overlays are meaningful, so several passes branch on it.
enum class WorldRenderMode {
  Rts,
  Rpg,
};

} // namespace Render::GL
