#pragma once

namespace Game::Systems {
class HealingBeamSystem;
} // namespace Game::Systems

namespace Render::GL {
class Renderer;
class ResourceManager;

/**
 * @brief Free function to render healing beams using the backend pipeline.
 *
 * This function integrates with the scene rendering pipeline.
 */
void render_healing_beams(Renderer *renderer, ResourceManager *resources,
                          const Game::Systems::HealingBeamSystem &beam_system);

} // namespace Render::GL
