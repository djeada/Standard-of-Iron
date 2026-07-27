#pragma once

#include <cstdint>

namespace Render::GL {

// Uniform block binding points are a single global namespace shared by every
// program.  Two blocks on the same point means whichever was bound last wins,
// silently and with no GL error -- the draw simply reads the wrong data.
//
// Keep every long-lived block listed here and never hard-code a number at a
// call site.  Two bugs came from doing exactly that: the shadow pass bound the
// bone palette over FrameData (binding 0), which replaced u_view_proj with bone
// matrices for every other shader and made all instanced world geometry vanish
// the moment a rigged unit cast a shadow; and the palette's own declared point
// collided with LocalLighting.
inline constexpr std::uint32_t k_frame_data_binding_point = 0;
inline constexpr std::uint32_t k_environment_lighting_binding_point = 1;
inline constexpr std::uint32_t k_local_lighting_binding_point = 2;
inline constexpr std::uint32_t k_directional_shadow_binding_point = 3;
inline constexpr std::uint32_t k_bone_palette_binding_point = 4;

} // namespace Render::GL
