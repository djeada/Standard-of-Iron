#version 330 core
#include "environment_lighting.glsl"
#include "noise.glsl"

in vec2 v_uv;

uniform sampler2D u_depth;
uniform vec2 u_depth_range;
uniform vec2 u_sun_screen;
uniform float u_sun_visibility;

out vec4 frag_color;

const int k_ray_samples = 28;
const float k_ray_density = 0.92;
const float k_ray_decay = 0.965;
const float k_ray_reach = 1.35;
const float k_sky_depth_fraction = 0.999;
const float k_source_falloff = 1.10;

float sky_mask(vec2 uv) {
  vec2 clamped = clamp(uv, vec2(0.0), vec2(1.0));
  float raw = texture(u_depth, clamped).r;
  float ndc = raw * 2.0 - 1.0;
  float near_plane = u_depth_range.x;
  float far_plane = u_depth_range.y;
  float linear = (2.0 * near_plane * far_plane) /
                 (far_plane + near_plane - ndc * (far_plane - near_plane));
  float sky = step(far_plane * k_sky_depth_fraction, linear);
  float inside = step(0.0, uv.x) * step(uv.x, 1.0) * step(0.0, uv.y) * step(uv.y, 1.0);
  return sky * inside;
}

void main() {
  if (u_sun_visibility <= 0.0) {
    frag_color = vec4(0.0);
    return;
  }
  vec2 to_sun = u_sun_screen - v_uv;
  float distance_to_sun = length(to_sun);
  vec2 step_uv = to_sun * (k_ray_density / float(k_ray_samples));
  float jitter = soi_hash13_a1b3c9(vec3(floor(v_uv * vec2(640.0, 360.0)), 3.0));
  vec2 sample_uv = v_uv + step_uv * jitter;
  float illumination = 0.0;
  float weight = 1.0;
  float weight_total = 0.0;
  for (int i = 0; i < k_ray_samples; ++i) {
    illumination += sky_mask(sample_uv) * weight;
    weight_total += weight;
    weight *= k_ray_decay;
    sample_uv += step_uv;
  }
  illumination /= max(weight_total, 1e-4);
  float source_falloff = 1.0 - smoothstep(0.0, k_ray_reach, distance_to_sun);
  source_falloff = pow(source_falloff, k_source_falloff);
  float rays = illumination * source_falloff * u_sun_visibility;
  frag_color = vec4(vec3(rays), 1.0);
}
