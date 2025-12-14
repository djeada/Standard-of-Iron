#pragma once

namespace Engine::Core {
class World;
} // namespace Engine::Core

namespace Render::GL {
class Renderer;
class ResourceManager;

/**
 * @brief Free function to render healer auras using the backend pipeline.
 *
 * This function integrates with the scene rendering pipeline by submitting
 * healer aura commands through the draw queue system.
 */
void render_healer_auras(Renderer *renderer, ResourceManager *resources,
                         Engine::Core::World *world);

} // namespace Render::GL
