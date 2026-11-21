#pragma once

namespace Engine::Core {
class World;
}

namespace Render::GL {
class FrameSubmitter;

void render_production_completions(FrameSubmitter* renderer,
                                   Engine::Core::World* world,
                                   int local_owner_id,
                                   bool reduced_motion);

void render_ripe_fields(FrameSubmitter* renderer,
                        Engine::Core::World* world,
                        int local_owner_id,
                        bool reduced_motion);

} // namespace Render::GL
