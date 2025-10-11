#pragma once

namespace Render {
namespace GL {
class EntityRendererRegistry;
}
} // namespace Render

namespace Render::GL {

void registerBarracksRenderer(EntityRendererRegistry &registry);

}
