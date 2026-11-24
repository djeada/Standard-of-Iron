#pragma once

namespace Render::GL {
class EntityRendererRegistry;
}

namespace Render::GL::Roman {

// Draws a home authored with "style": "tenement" (the insula).
void register_tenement_renderer(Render::GL::EntityRendererRegistry& registry);

} // namespace Render::GL::Roman
