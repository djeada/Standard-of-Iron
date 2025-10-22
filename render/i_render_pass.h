#pragma once

namespace Render {
namespace GL {

class Renderer;
class ResourceManager;

struct IRenderPass {
  virtual ~IRenderPass() = default;
  virtual void submit(Renderer &renderer, ResourceManager *resources) = 0;
};

} // namespace GL
} // namespace Render
