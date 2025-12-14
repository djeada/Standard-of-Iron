#pragma once

namespace Game::Systems {
class HealingBeamSystem;
}

namespace Render::GL {
class Renderer;
class ResourceManager;

void render_healing_beams(Renderer *renderer, ResourceManager *resources,
                          const Game::Systems::HealingBeamSystem &beam_system);

} // namespace Render::GL
