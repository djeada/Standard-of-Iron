#pragma once

namespace Render::GL {

class Shader;
class Texture;

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
