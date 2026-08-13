#version 330 core
#include "tonemap.glsl"

in vec2 v_uv;

uniform sampler2D u_scene;
uniform sampler2D u_bloom;
uniform sampler2D u_depth;
uniform float u_bloom_intensity;
uniform float u_vignette_strength;
uniform vec2 u_depth_range;
uniform vec2 u_inverse_resolution;
uniform float u_ground_ao_radius;
uniform float u_ground_ao_strength;

out vec4 frag_color;

const float k_vignette_inner = 0.55;
const float k_vignette_outer = 1.35;
const vec3 k_vignette_tint = vec3(0.86, 0.89, 0.97);
const int k_ao_tap_count = 8;
const float k_ao_spread = 3.0;
const vec3 k_ao_tint = vec3(0.72, 0.76, 0.86);

const vec2 k_ao_taps[8] = vec2[8](vec2(1.0, 0.0),
                                  vec2(0.7071, 0.7071),
                                  vec2(0.0, 1.0),
                                  vec2(-0.7071, 0.7071),
                                  vec2(-1.0, 0.0),
                                  vec2(-0.7071, -0.7071),
                                  vec2(0.0, -1.0),
                                  vec2(0.7071, -0.7071));

float linear_depth(vec2 uv) {
  float raw = texture(u_depth, uv).r;
  float near_plane = u_depth_range.x;
  float far_plane = u_depth_range.y;
  float ndc = raw * 2.0 - 1.0;
  return (2.0 * near_plane * far_plane) /
         (far_plane + near_plane - ndc * (far_plane - near_plane));
}

float grounding_occlusion() {
  float center = linear_depth(v_uv);
  if (center >= u_depth_range.y * 0.999) {
    return 0.0;
  }

  float occlusion = 0.0;
  for (int tap = 0; tap < k_ao_tap_count; ++tap) {
    vec2 offset = k_ao_taps[tap] * u_inverse_resolution * k_ao_spread;
    float neighbour = linear_depth(v_uv + offset);
    float difference = center - neighbour;
    float contribution =
        smoothstep(0.0, u_ground_ao_radius, difference) *
        (1.0 - smoothstep(u_ground_ao_radius, u_ground_ao_radius * 6.0, difference));
    occlusion += contribution;
  }
  return clamp(occlusion / float(k_ao_tap_count), 0.0, 1.0);
}

void main() {
  vec3 scene = texture(u_scene, v_uv).rgb;
  vec3 bloom = texture(u_bloom, v_uv).rgb;
  vec3 combined = scene + bloom * u_bloom_intensity;

  float occlusion = grounding_occlusion() * u_ground_ao_strength;
  combined = mix(combined, combined * k_ao_tint, occlusion);

  vec3 graded = soi_finalize(combined);

  vec2 centered = v_uv * 2.0 - 1.0;
  float falloff = smoothstep(k_vignette_inner, k_vignette_outer, length(centered));
  float vignette = 1.0 - falloff * u_vignette_strength;
  graded = mix(graded * k_vignette_tint, graded, vignette);
  graded *= vignette;

  frag_color = vec4(clamp(graded, 0.0, 1.0), 1.0);
}
