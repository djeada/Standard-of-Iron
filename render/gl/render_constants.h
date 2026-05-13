#pragma once

#include <cstdint>
#include <numbers>

namespace Render::GL::MathConstants {
inline constexpr float k_pi = std::numbers::pi_v<float>;
inline constexpr float k_two_pi = std::numbers::pi_v<float> * 2.0F;
} // namespace Render::GL::MathConstants

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

namespace Render::GL::BufferCapacity {
inline constexpr int default_cylinder_instances = 256;
inline constexpr int default_fog_instances = 512;
inline constexpr int buffers_in_flight = 3;
inline constexpr int shader_info_log_size = 512;
} // namespace Render::GL::BufferCapacity

namespace Render::GL::Geometry {
inline constexpr int quad_vertex_count = 4;
inline constexpr int quad_index_count = 6;
inline constexpr int grass_blade_vertex_count = 6;
inline constexpr int cube_vertex_count = 24;
inline constexpr int cube_index_count = 36;
inline constexpr int plant_cross_quad_vertex_count = 24;
inline constexpr int plant_cross_quad_index_count = 36;
inline constexpr int pine_tree_segments = 6;
inline constexpr int olive_tree_segments = 8;
inline constexpr int ground_plane_subdivisions = 128;
inline constexpr int default_capsule_segments = 8;
inline constexpr int default_torso_height_segments = 8;
inline constexpr int default_chunk_size = 16;
inline constexpr int min_length_steps = 8;
inline constexpr int min_length_segments = 8;
} // namespace Render::GL::Geometry

namespace Render::GL::Growth {
inline constexpr int capacity_multiplier = 2;
}

namespace Render::GL::ColorIndex {
inline constexpr int red = 0;
inline constexpr int green = 1;
inline constexpr int blue = 2;
inline constexpr int alpha = 3;
} // namespace Render::GL::ColorIndex

namespace Render::GL::FrustumPlane {
inline constexpr int matrix_size = 16;
inline constexpr int index_0 = 0;
inline constexpr int index_1 = 1;
inline constexpr int index_2 = 2;
inline constexpr int index_3 = 3;
inline constexpr int index_4 = 4;
inline constexpr int index_5 = 5;
inline constexpr int index_6 = 6;
inline constexpr int index_7 = 7;
inline constexpr int index_8 = 8;
inline constexpr int index_9 = 9;
inline constexpr int index_10 = 10;
inline constexpr int index_11 = 11;
} // namespace Render::GL::FrustumPlane

namespace Render::GL::RGBA {
inline constexpr unsigned char max_value = 255;
inline constexpr unsigned char min_value = 0;
} // namespace Render::GL::RGBA

namespace Render::GL::BitShift {
inline constexpr int shift_8 = 8;
inline constexpr int shift_16 = 16;
inline constexpr int shift_32 = 32;
inline constexpr unsigned int mask_24_bit = 0xFFFFFF;
inline constexpr float mask_24_bit_float = 16777215.0F;
inline constexpr unsigned int k_mask_24bit_hex = 0x01000000U;
} // namespace Render::GL::BitShift

namespace Render::GL::HashXorShift {
inline constexpr int k_xor_shift_amount_5 = 5;
inline constexpr int k_xor_shift_amount_13 = 13;
inline constexpr int k_xor_shift_amount_17 = 17;
inline constexpr uint32_t k_golden_ratio = 0x9E3779B9U;
} // namespace Render::GL::HashXorShift
