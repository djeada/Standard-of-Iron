// Stage 16.2 — shared UBO binding-point allocator.
//
// Centralises every `layout(std140) uniform Foo { ... }` binding point so
// adding a new UBO is a single-line change and conflicts surface at code
// review time. Bindings 0 and 1 are reserved for future
// camera/lights blocks; the bone-palette UBO sits at slot 2.
//
// The values must match the explicit `layout(binding=N)` decoration in
// each shader (or the `glUniformBlockBinding` call performed at
// pipeline init for shaders that lack the GLSL 4.20+ binding syntax).

#pragma once

#include <cstdint>

namespace Render::GL {

inline constexpr std::uint32_t kBonePaletteBindingPoint = 2;

} // namespace Render::GL
