#pragma once

namespace Render::GL {

class Shader;
class Texture;

// The GL binding state the command executors track across a frame so they can
// skip redundant binds. Owned by the backend and reset at the start of each
// frame.
struct GlStateCache {
  Shader* bound_shader{nullptr};
  Texture* bound_texture{nullptr};
  bool polygon_offset_enabled{false};

  void forget_bindings() noexcept {
    bound_shader = nullptr;
    bound_texture = nullptr;
  }
};

} // namespace Render::GL
