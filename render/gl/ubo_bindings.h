#pragma once

#include <cstdint>

namespace Render::GL {

inline constexpr std::uint32_t k_frame_data_binding_point = 0;
inline constexpr std::uint32_t k_environment_lighting_binding_point = 1;
inline constexpr std::uint32_t k_local_lighting_binding_point = 2;
inline constexpr std::uint32_t k_directional_shadow_binding_point = 3;
inline constexpr std::uint32_t k_bone_palette_binding_point = 4;

} // namespace Render::GL
