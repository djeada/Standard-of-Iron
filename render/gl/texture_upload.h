#pragma once

namespace Render::GL {

enum class FloatTexelLayout {
  Red,
  Rgba
};

void upload_bound_texture_rgba8(
    int x, int y, int width, int height, const void* texels);

void upload_bound_texture_float(int width,
                                int height,
                                FloatTexelLayout layout,
                                const float* texels);

} // namespace Render::GL
