#pragma once

#include "render/math/render_constants.h"

namespace Render::GL::VertexAttrib {
inline constexpr int position = 0;
inline constexpr int normal = 1;
inline constexpr int tex_coord = 2;
inline constexpr int instance_position = 3;
inline constexpr int instance_scale = 4;
inline constexpr int instance_color = 5;
inline constexpr int instance_alpha = 6;
inline constexpr int instance_tint = 7;
} // namespace Render::GL::VertexAttrib

namespace Render::GL::ComponentCount {
inline constexpr int vec2 = 2;
inline constexpr int vec3 = 3;
inline constexpr int vec4 = 4;
} // namespace Render::GL::ComponentCount

namespace Render::GL::TextureUnit {
inline constexpr int terrain_height = 6;
inline constexpr int terrain_visibility = 7;
inline constexpr int directional_shadow_map = 8;
inline constexpr int directional_shadow_map_far = 9;
inline constexpr int terrain_fields = 10;
inline constexpr int terrain_noise_atlas = 11;

inline constexpr int directional_shadow_depth = 12;
inline constexpr int terrain_noise_atlas_detail = 13;
inline constexpr int terrain_microdetail = 14;
inline constexpr int material_detail = 15;
inline constexpr int character_wear_volume = 16;
inline constexpr int terrain_cover = 17;
} // namespace Render::GL::TextureUnit

namespace Render::GL::BufferCapacity {
inline constexpr int default_cylinder_instances = 256;
inline constexpr int default_fog_instances = 512;
inline constexpr int buffers_in_flight = 3;

inline constexpr int max_buffers_in_flight = 4;
inline constexpr int shader_info_log_size = 512;
} // namespace Render::GL::BufferCapacity
