#version 330 core
#include "noise.glsl"
#include "terrain_noise.glsl"

in vec2 v_uv;

layout(location = 0) out vec4 frag_fields;

uniform vec2 u_bake_world_min;
uniform vec2 u_bake_world_size;
uniform vec2 u_noise_offset;
uniform float u_tile_size;
uniform float u_macro_noise_scale;

const int k_bake_octaves = 5;
const float k_bake_footprint = 0.0;

float bake_fbm(vec2 p) {
  return soi_terrain_gradient_fbm_7c25da(p, k_bake_octaves, k_bake_footprint);
}

void main() {
  vec2 world_xz = u_bake_world_min + v_uv * u_bake_world_size;
  float tile_scale = max(u_tile_size, 0.0001);
  vec2 world_coord = (world_xz / tile_scale) + u_noise_offset;
  float macro_scale = max(u_macro_noise_scale, 0.010);

  vec2 domain_warp = vec2(bake_fbm(world_coord * macro_scale * 0.43 + 13.7),
                          bake_fbm(world_coord * macro_scale * 0.43 - 9.2));

  float regional_field = clamp(
      0.5 + bake_fbm(world_coord * macro_scale * 0.56 + domain_warp * 0.85) * 0.72,
      0.0,
      1.0);
  float soil_field = clamp(
      0.5 +
          bake_fbm(world_coord * macro_scale * 4.80 + domain_warp * 0.65 + 31.0) * 0.72,
      0.0,
      1.0);
  float moisture_field = clamp(
      0.5 +
          bake_fbm(world_coord * macro_scale * 1.80 - domain_warp * 0.60 + 73.0) * 0.68,
      0.0,
      1.0);
  float meadow_field = clamp(0.5 + bake_fbm(world_coord * macro_scale * 1.05 +
                                            domain_warp * 0.48 + vec2(-47.0, 26.0)) *
                                       0.70,
                             0.0,
                             1.0);

  frag_fields = vec4(regional_field, soil_field, moisture_field, meadow_field);
}
