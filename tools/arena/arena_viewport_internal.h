#pragma once

namespace Game::Map {
class VisibilityService;
}

namespace Render::GL {
class Camera;
}

namespace arena_viewport_internal {

inline constexpr int k_local_owner_id = 1;
inline constexpr int k_enemy_owner_id = 2;
inline constexpr int k_terrain_width = 128;
inline constexpr int k_terrain_height = 128;
inline constexpr float k_terrain_tile_size = 1.0F;
inline constexpr float k_default_floor_extent = 18.0F;
inline constexpr float k_default_terrain_height_scale = 6.0F;
inline constexpr int k_selection_drag_threshold = 6;

void sync_camera_map_bounds(Render::GL::Camera* camera,
                            const Game::Map::VisibilityService& visibility);

} // namespace arena_viewport_internal
