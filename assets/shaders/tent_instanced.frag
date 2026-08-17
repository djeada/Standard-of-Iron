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
in vec3 v_local_normal;

out vec4 frag_color;

const vec3 k_tent_lantern_color = vec3(1.0, 0.58, 0.24);
const float k_tent_lantern_gain = 0.55;

float band(float value, float center, float half_width, float feather) {
  return 1.0 - smoothstep(half_width, half_width + feather, abs(value - center));
}

void main() {
  vec3 N = normalize(v_normal);
  vec3 L = environment_primary_direction();
  vec3 V = normalize(vec3(0.0, 0.86, 0.52));
  vec3 H = normalize(L + V);

  float front_plane = band(abs(v_local_pos.z), 0.60, 0.035, 0.018);
  float front_posts = front_plane * band(abs(v_local_pos.x), 0.20, 0.055, 0.025) *
                      (1.0 - smoothstep(0.48, 0.56, v_local_pos.y));
  float front_crossbar = front_plane * band(v_local_pos.y, 0.44, 0.045, 0.025) *
                         (1.0 - smoothstep(0.27, 0.34, abs(v_local_pos.x)));
  float center_pole =
      (1.0 - smoothstep(0.035, 0.072, max(abs(v_local_pos.x), abs(v_local_pos.z)))) *
      (1.0 - smoothstep(0.76, 0.84, v_local_pos.y));
  float awning_posts = band(abs(v_local_pos.x), 0.446, 0.035, 0.020) *
                       band(v_local_pos.z, -0.88, 0.045, 0.025) *
                       (1.0 - smoothstep(0.43, 0.50, v_local_pos.y));
  float stakes = (1.0 - smoothstep(0.00, 0.10, v_local_pos.y)) *
                 smoothstep(0.59, 0.66, max(abs(v_local_pos.x), abs(v_local_pos.z)));
  float wood_mask = clamp(
      max(max(front_posts, front_crossbar), max(center_pole, awning_posts)), 0.0, 1.0);
  float iron_mask = stakes * (1.0 - wood_mask);
  float canvas_mask = 1.0 - max(wood_mask, iron_mask);

  float weave_a = 0.5 + 0.5 * sin((v_local_pos.x + v_local_pos.y * 0.32) * 92.0);
  float weave_b = 0.5 + 0.5 * sin((v_local_pos.z - v_local_pos.y * 0.27) * 87.0);
  float weave = weave_a * weave_b;
  float panel_seam = band(fract((v_local_pos.z + 0.61) * 2.35), 0.5, 0.028, 0.018);
  float ridge_seam =
      band(v_local_pos.x, 0.0, 0.018, 0.020) * smoothstep(0.44, 0.72, v_local_pos.y);
  float seam = max(panel_seam * 0.55, ridge_seam);
  float dye_mottle =
      soi_hash12_9f6e8e(floor(v_world_pos.xz * 3.0) + floor(v_local_pos.xy * 9.0));

  vec3 canvas = v_color * mix(0.78, 1.16, weave * 0.40 + dye_mottle * 0.60);
  canvas = mix(canvas, canvas * vec3(0.62, 0.58, 0.52), seam * 0.60);
  float sun_bleach =
      smoothstep(0.50, 0.88, v_local_pos.y) * smoothstep(0.18, 0.76, v_local_normal.y);
  canvas = mix(canvas, canvas + vec3(0.16, 0.13, 0.09), sun_bleach * 0.25);
  float base_dirt = (1.0 - smoothstep(0.03, 0.30, v_local_pos.y)) *
                    (0.55 + soi_hash12_9f6e8e(v_world_pos.xz * 1.7) * 0.45);
  canvas = mix(canvas, vec3(0.25, 0.19, 0.13), base_dirt * 0.48);

  float grain = 0.5 + 0.5 * sin(v_local_pos.y * 34.0 + v_local_pos.x * 9.0);
  vec3 timber = mix(vec3(0.20, 0.105, 0.045), vec3(0.48, 0.29, 0.10), grain);
  vec3 iron = mix(vec3(0.10, 0.11, 0.12),
                  vec3(0.28, 0.25, 0.21),
                  soi_hash12_9f6e8e(v_world_pos.xz * 7.0));
  vec3 albedo = mix(canvas, timber, wood_mask);
  albedo = mix(albedo, iron, iron_mask);

  float ndotl = max(dot(N, L), 0.0);
  float hemi = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
  vec3 sky = environment_sky_color();
  vec3 sun = environment_primary_color() * environment_primary_intensity();
  vec3 illumination = soi_surface_lighting_scaled(N, 0.72);
  float ao = mix(0.55, 1.0, hemi) * mix(1.0, 0.78, base_dirt);
  float specular = pow(max(dot(N, H), 0.0), 58.0) * mix(0.025, 0.20, iron_mask);
  float rim = pow(1.0 - max(dot(N, V), 0.0), 4.0) * 0.045;

  vec3 color = albedo * illumination * ao;
  color += sun * specular;
  color += sky * rim;
  color += albedo * ao * local_lighting(v_world_pos, normalize(v_normal));
  color = apply_directional_shadow(color, v_world_pos, v_normal);
  float night = environment_night_amount();
  if (night > 0.0) {
    float interior = (1.0 - smoothstep(0.10, 0.62, v_local_pos.y)) *
                     smoothstep(0.02, 0.16, v_local_pos.y);
    float lantern_flicker =
        0.86 + 0.14 * sin(v_world_pos.x * 3.7 + v_world_pos.z * 2.9 +
                          soi_hash12_9f6e8e(floor(v_world_pos.xz)) * 6.2831);
    vec3 lantern_glow = k_tent_lantern_color * canvas_mask * interior *
                        (0.55 + 0.45 * weave) * (1.0 - base_dirt * 0.7) *
                        (1.0 - seam * 0.5) * lantern_flicker;
    color += lantern_glow * k_tent_lantern_gain * night;
  }
  color = apply_visibility_memory(color, v_world_pos.xz);
  frag_color = vec4(color, 1.0);
}
