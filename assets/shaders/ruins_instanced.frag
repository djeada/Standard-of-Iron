#version 330 core
#include "directional_shadows.glsl"
#include "environment_lighting.glsl"
#include "local_lighting.glsl"
#include "noise.glsl"
#include "visibility_mask.glsl"

in vec3 v_world_pos;
in vec3 v_normal;
in vec3 v_color;
in vec3 v_local_pos;

out vec4 frag_color;

void main() {
  vec3 N = normalize(v_normal);
  vec3 L = environment_primary_direction();
  vec3 V = normalize(vec3(0.0, 0.86, 0.52));
  vec3 H = normalize(L + V);

  float coarse = soi_noise21_b0e82b(v_world_pos.xz * 1.35 + v_local_pos.xy * 2.1);
  float pits = soi_noise21_b0e82b(v_world_pos.xz * 6.5 + v_local_pos.zy * 5.0);
  float bedding = 0.5 + 0.5 * sin(v_local_pos.y * 17.0 + coarse * 4.2);
  vec3 stone = v_color * mix(0.67, 1.06, coarse);
  stone *= mix(0.82, 1.05, bedding * 0.55 + pits * 0.45);

  float upward = smoothstep(0.32, 0.88, N.y);
  float lichen_field =
      soi_noise21_b0e82b(v_world_pos.xz * 2.7 + vec2(v_local_pos.y * 1.3));
  float lichen = upward * smoothstep(0.54, 0.82, lichen_field) * 0.42;
  stone = mix(stone, vec3(0.22, 0.27, 0.20), lichen);

  float vertical_face = 1.0 - smoothstep(0.35, 0.82, abs(N.y));
  float rain_path = soi_noise21_b0e82b(
      vec2(v_world_pos.x * 2.2 + v_world_pos.z, floor(v_local_pos.y * 3.0) * 0.37));
  float rain_stain = vertical_face * smoothstep(0.60, 0.88, rain_path) *
                     (1.0 - smoothstep(0.35, 1.65, v_local_pos.y));
  stone = mix(stone, stone * vec3(0.48, 0.54, 0.53), rain_stain * 0.60);

  float fracture_field = abs(sin(v_local_pos.x * 15.0 - v_local_pos.z * 11.0 +
                                 v_local_pos.y * 8.0 + coarse * 3.0));
  float fracture = 1.0 - smoothstep(0.025, 0.11, fracture_field);
  stone *= mix(1.0, 0.58, fracture * 0.55);

  float base_stain = 1.0 - smoothstep(0.02, 0.38, v_local_pos.y);
  stone = mix(stone, stone * vec3(0.48, 0.54, 0.50), base_stain * 0.50);

  vec2 sun_flat = normalize(L.xz + vec2(1e-4, 0.0));
  float shaded_face =
      clamp(dot(normalize(N.xz + vec2(1e-4, 0.0)), -sun_flat), 0.0, 1.0);
  float ivy_field = soi_noise21_b0e82b(v_world_pos.xz * 1.6 + v_local_pos.xy * 3.4 +
                                       vec2(v_local_pos.z * 2.2, 17.0));
  float ivy_reach = 1.0 - smoothstep(0.30, 1.30 + ivy_field * 0.9, v_local_pos.y);
  float ivy = ivy_reach * (0.35 + 0.65 * shaded_face) * vertical_face *
              smoothstep(0.42, 0.72, ivy_field + base_stain * 0.25);
  vec3 ivy_color = mix(vec3(0.16, 0.27, 0.12), vec3(0.30, 0.42, 0.16), pits);
  stone = mix(stone, ivy_color, ivy * 0.78);

  float ndotl = max(dot(N, L), 0.0);
  float hemi = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
  vec3 sky = environment_sky_color();
  vec3 sun = environment_primary_color() * environment_primary_intensity();
  vec3 illumination = soi_surface_lighting_scaled(N, 0.68);
  float ao = mix(0.48, 1.0, hemi) * mix(1.0, 0.68, fracture);
  float specular = pow(max(dot(N, H), 0.0), 28.0) * (0.025 + rain_stain * 0.09);
  float rim = pow(1.0 - max(dot(N, V), 0.0), 4.0) * 0.045;

  vec3 color = stone * illumination * ao;
  color += sun * specular;
  color += sky * rim;
  color = apply_directional_shadow(color, v_world_pos, v_normal);
  color += stone * ao * local_lighting(v_world_pos, normalize(v_normal));
  color = apply_visibility_world_shading(color, v_world_pos.xz);
  frag_color = vec4(color, 1.0);
}
