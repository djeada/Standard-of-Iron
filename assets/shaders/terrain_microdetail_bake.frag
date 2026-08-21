#version 330 core
#include "noise.glsl"
#include "terrain_noise.glsl"

in vec2 v_uv;

layout(location = 0) out vec4 frag_microdetail;

uniform float u_microdetail_cells;

const int k_microdetail_octaves = 5;

const float k_microdetail_gradient_step = 0.14;

void main() {
  float cells = max(u_microdetail_cells, 1.0);
  vec2 period = vec2(cells);
  vec2 p = v_uv * cells;

  float single = soi_terrain_gradient_noise_tiled_4c19af(p, period);
  float shifted_x = soi_terrain_gradient_noise_tiled_4c19af(
      p + vec2(k_microdetail_gradient_step, 0.0), period);
  float shifted_y = soi_terrain_gradient_noise_tiled_4c19af(
      p + vec2(0.0, k_microdetail_gradient_step), period);
  float fbm = soi_terrain_gradient_fbm_tiled_9d22b1(p, period, k_microdetail_octaves);

  frag_microdetail = vec4(single,
                          fbm,
                          (shifted_x - single) / k_microdetail_gradient_step,
                          (shifted_y - single) / k_microdetail_gradient_step);
}
