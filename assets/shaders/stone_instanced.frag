#version 330 core
#include "directional_shadows.glsl"
#include "environment_lighting.glsl"
#include "local_lighting.glsl"
#include "visibility_mask.glsl"

in vec3 v_world_pos;
in vec3 v_normal;
in vec3 v_color;
in vec3 v_local_pos;
in float v_ground_height;
flat in float v_scale;
flat in float v_seed;

uniform vec3 u_camera_pos;

out vec4 frag_color;

float hash31(vec3 p) {
  p = fract(p * 0.1031);
  p += dot(p, p.yzx + 33.33);
  return fract((p.x + p.y) * p.z);
}

float stone_noise3(vec3 p) {
  vec3 i = floor(p);
  vec3 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  return mix(
      mix(mix(hash31(i), hash31(i + vec3(1.0, 0.0, 0.0)), f.x),
          mix(hash31(i + vec3(0.0, 1.0, 0.0)), hash31(i + vec3(1.0, 1.0, 0.0)), f.x),
          f.y),
      mix(mix(hash31(i + vec3(0.0, 0.0, 1.0)), hash31(i + vec3(1.0, 0.0, 1.0)), f.x),
          mix(hash31(i + vec3(0.0, 1.0, 1.0)), hash31(i + vec3(1.0, 1.0, 1.0)), f.x),
          f.y),
      f.z);
}

vec3 relief_normal(vec3 N, vec3 p, float strength) {
  const float eps = 0.045;
  vec3 q = p * 6.5 + vec3(v_seed * 19.0);
  float gx =
      stone_noise3(q + vec3(eps, 0.0, 0.0)) - stone_noise3(q - vec3(eps, 0.0, 0.0));
  float gy =
      stone_noise3(q + vec3(0.0, eps, 0.0)) - stone_noise3(q - vec3(0.0, eps, 0.0));
  float gz =
      stone_noise3(q + vec3(0.0, 0.0, eps)) - stone_noise3(q - vec3(0.0, 0.0, eps));
  vec3 grad = vec3(gx, gy, gz) / (2.0 * eps);
  grad -= N * dot(grad, N);
  return normalize(N - grad * strength);
}

void main() {
  vec3 N_face = normalize(v_normal);
  vec3 L = environment_primary_direction();
  vec3 V = normalize(u_camera_pos - v_world_pos);
  vec3 H = normalize(L + V);

  vec3 p = v_local_pos;
  vec3 seed_offset = vec3(v_seed * 7.3, v_seed * 3.1, v_seed * 11.7);
  float broad = stone_noise3(p * 3.2 + vec3(1.7, 5.1, 2.3) + seed_offset);
  float grain = stone_noise3(p * 13.0 + v_world_pos * 0.12);
  float speckle = stone_noise3(p * 31.0 + seed_offset);
  float strata = 0.5 + 0.5 * sin(p.y * 22.0 + p.x * 4.0 + broad * 3.2 + v_seed * 6.0);
  float fissure_field =
      abs(sin(p.x * 13.0 - p.z * 9.0 + p.y * 6.0 + broad * 4.0 + v_seed * 4.0));
  float fissures = 1.0 - smoothstep(0.035, 0.14, fissure_field);

  vec3 N = relief_normal(N_face, p, 0.10);

  vec3 stone = v_color * mix(0.70, 1.08, broad);
  stone *= mix(0.86, 1.08, grain * 0.65 + strata * 0.35);
  stone *= mix(0.93, 1.05, speckle);
  stone = mix(stone, stone * vec3(0.58, 0.58, 0.57), fissures * 0.72);

  float upward = smoothstep(0.28, 0.86, N.y);
  float lichen_noise = stone_noise3(v_world_pos * 1.8 + vec3(3.0, 8.0, 1.0));
  float lichen = upward * smoothstep(0.58, 0.82, lichen_noise) * 0.34;
  vec3 lichen_color = vec3(0.30, 0.33, 0.22);
  stone = mix(stone, lichen_color, lichen);

  float shade_side = 1.0 - smoothstep(-0.1, 0.55, dot(N_face, L));
  float foot = 1.0 - smoothstep(0.0, 0.42 * v_scale, v_ground_height);
  float moss_noise = stone_noise3(v_world_pos * 2.6 + vec3(11.0, 2.0, 5.0));
  float moss = smoothstep(0.50, 0.78, moss_noise) * (shade_side * 0.55 + foot * 0.45) *
               smoothstep(-0.2, 0.5, N.y) * 0.42;
  vec3 moss_color = vec3(0.17, 0.25, 0.13);
  stone = mix(stone, moss_color, moss);

  float skirt = (1.0 - smoothstep(0.0, 0.16 * v_scale, v_ground_height)) *
                mix(0.55, 1.0, stone_noise3(v_world_pos * 4.0));
  vec3 earth_color = vec3(0.30, 0.25, 0.19);
  stone = mix(stone, mix(stone, earth_color, 0.55), skirt * 0.75);
  float ground_damp = skirt * smoothstep(0.28, 0.72, stone_noise3(v_world_pos * 0.9));
  stone = mix(stone, stone * vec3(0.66, 0.66, 0.64), ground_damp * 0.5);
  float rain_damp =
      environment_wetness() * smoothstep(0.02, 0.78, N.y) * mix(0.62, 1.0, broad);
  stone *= 1.0 - rain_damp * 0.20;

  float hemi = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
  vec3 sky = environment_sky_color();
  vec3 sun = environment_primary_color() * environment_primary_intensity();
  vec3 illumination = soi_surface_lighting_scaled(N, 0.72);
  float crevice_ao =
      mix(1.0, 0.62, fissures) * mix(0.58, 1.0, hemi) * mix(1.0, 0.72, skirt);

  float wet_surface = max(ground_damp, rain_damp);
  float wet_spec = wet_surface * pow(max(dot(N, H), 0.0), 38.0) * 0.22;
  float dry_spec = pow(max(dot(N, H), 0.0), 18.0) * 0.025 * (1.0 - moss);
  float rim = pow(1.0 - max(dot(N, V), 0.0), 4.0) * 0.055;

  vec3 color = stone * illumination * crevice_ao;
  color += soi_rim_light(N_face, V) * (1.0 - skirt * 0.6);
  color += sun * (dry_spec + wet_spec);
  color += sky * rim;
  color = apply_directional_shadow(color, v_world_pos, v_normal);
  color += stone * crevice_ao * local_lighting(v_world_pos, N);
  color = apply_visibility_memory(color, v_world_pos.xz);
  frag_color = vec4(color, 1.0);
}
