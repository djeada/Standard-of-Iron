#pragma once

namespace Engine::Core {

class World;

void begin_motion_presentation_frame(World& world, float delta_time);

void finalize_motion_presentation_frame(World& world, float delta_time);

void publish_movement_trace_frame(World& world, float delta_time);

} // namespace Engine::Core
