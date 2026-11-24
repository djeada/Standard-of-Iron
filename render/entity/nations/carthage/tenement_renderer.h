#pragma once

namespace Render::GL {
class EntityRendererRegistry;
}

namespace Render::GL::Carthage {

// Draws a home authored with "style": "tenement" (the Punic tall house).
void register_tenement_renderer(Render::GL::EntityRendererRegistry& registry);

} // namespace Render::GL::Carthage
