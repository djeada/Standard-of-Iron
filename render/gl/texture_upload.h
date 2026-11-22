#pragma once

namespace Render::GL {

enum class FloatTexelLayout {
  Red,
  Rgba
};

// Writes a sub-rectangle of 8-bit RGBA texels into the 2D texture currently
// bound on the active unit, and counts the transfer.
void upload_bound_texture_rgba8(
    int x, int y, int width, int height, const void* texels);

// Writes the whole width x height float image into the 2D texture currently
// bound on the active unit, and counts the transfer.
void upload_bound_texture_float(int width,
                                int height,
                                FloatTexelLayout layout,
                                const float* texels);

} // namespace Render::GL
