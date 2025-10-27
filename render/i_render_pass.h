#pragma once

namespace Render::GL {

class Renderer;
class ResourceManager;

struct IRenderPass {
  virtual ~IRenderPass() = default;
  virtual void submit(Renderer &renderer, ResourceManager *resources) = 0;
};

} // namespace Render::GL
