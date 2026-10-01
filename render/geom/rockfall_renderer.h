#pragma once

namespace Render::GL {
class Renderer;
} // namespace Render::GL

namespace Game::Systems {
struct RenderEffectsFrame;
} // namespace Game::Systems

namespace Render::GL {

// Draws rockfall boulders with the catapult stone mesh at boulder scale, plus
// the dust they kick up when released, landing, striking troops and rolling.
void render_rockfall(Renderer* renderer, const Game::Systems::RenderEffectsFrame& effects);

} // namespace Render::GL
