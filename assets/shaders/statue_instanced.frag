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

  float height = v_local_pos.y;
  float figure = smoothstep(1.168, 1.184, height);
  float lateral = min(abs(v_local_pos.x), abs(v_local_pos.z));

  float grain = soi_noise21_b0e82b(v_local_pos.xz * 31.0 + vec2(height * 23.0));
  float mottle =
      soi_noise21_b0e82b(v_local_pos.xz * 8.5 + vec2(height * 5.1, height * 3.3));
  vec2 vein_uv =
      vec2(v_local_pos.x * 6.4 + height * 2.6, v_local_pos.z * 6.4 - height * 1.8);
  float vein_field =
      soi_noise21_b0e82b(vein_uv) * 0.62 + soi_noise21_b0e82b(vein_uv * 2.9) * 0.38;
  float vein = 1.0 - smoothstep(0.0, 0.085, abs(vein_field - 0.5));

  vec3 stone = v_color;
  stone *= mix(0.966, 1.034, grain);
  stone *= mix(0.930, 1.048, mottle);
  stone = mix(stone, stone * vec3(0.80, 0.80, 0.84), vein * 0.55);
  stone = mix(stone * vec3(0.905, 0.910, 0.905), stone, figure);

  float to_panel_top = 0.828 - height;
  float to_panel_bottom = height - 0.446;
  float to_panel_side = 0.196 - lateral;
  float panel_inset = min(min(to_panel_top, to_panel_bottom), to_panel_side);
  float panel = (1.0 - figure) * smoothstep(0.0, 0.008, panel_inset);
  float border = panel * (1.0 - smoothstep(0.008, 0.036, panel_inset));
  float top_edge = step(to_panel_top, min(to_panel_bottom, to_panel_side));
  float bottom_edge = step(to_panel_bottom, min(to_panel_top, to_panel_side));

  stone *= mix(1.0, 0.945, panel);
  stone *= mix(1.0, 0.90, panel * smoothstep(0.70, 0.83, height));
  stone *= 1.0 + border * (bottom_edge * 0.24 - top_edge * 0.32);

  float vertical = 1.0 - smoothstep(0.28, 0.72, abs(N.y));
  float streak_field = soi_noise21_b0e82b(
      vec2(v_local_pos.x * 21.0 + v_local_pos.z * 18.0, height * 0.85));
  float streaks = vertical * smoothstep(0.56, 0.86, streak_field);
  stone = mix(stone, stone * vec3(0.80, 0.82, 0.80), streaks * 0.26);

  float upward = smoothstep(0.32, 0.84, N.y);
  float grime_field = soi_noise21_b0e82b(v_world_pos.xz * 3.4 + vec2(height * 1.6));
  float grime = upward * smoothstep(0.40, 0.80, grime_field);
  stone = mix(stone, vec3(0.46, 0.48, 0.40), grime * 0.22 * (1.0 - figure * 0.55));

  float base_moss_field = soi_noise21_b0e82b(
      v_world_pos.xz * 2.3 + vec2(v_local_pos.y * 3.1, v_local_pos.x * 2.7));
  float moss_reach = 1.0 - smoothstep(0.02, 0.36 + base_moss_field * 0.30, height);
  float base_moss =
      moss_reach * (1.0 - figure * 0.6) * smoothstep(0.38, 0.70, base_moss_field);
  stone = mix(stone, vec3(0.20, 0.30, 0.14), base_moss * 0.62);

  float splash = 1.0 - smoothstep(0.02, 0.34, height);
  float moss_field = soi_noise21_b0e82b(v_world_pos.xz * 5.4 + vec2(height * 3.1));
  stone = mix(stone,
              vec3(0.34, 0.39, 0.26),
              splash * smoothstep(0.42, 0.80, moss_field) * 0.34);
  stone *= mix(1.0, 0.88, splash * 0.70);

  float ndotl = dot(N, L);
  float lambert = max(ndotl, 0.0);
  float wrapped = max(ndotl * 0.42 + 0.58, 0.0);
  float hemi = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);

  vec3 sky = environment_sky_color();
  vec3 sun = environment_primary_color() * environment_primary_intensity();

  float translucency = pow(clamp(ndotl * 0.35 + 0.65, 0.0, 1.0), 2.2);
  vec3 subsurface =
      sun * vec3(1.00, 0.93, 0.84) * translucency * 0.24 * mix(0.7, 1.0, figure);

  float cavity = mix(0.85, 1.0, hemi);
  cavity *= mix(1.0, 0.93, panel);
  cavity *= mix(1.0, 0.95, streaks);
  cavity *= mix(1.0, 0.92, grime);

  vec3 illumination = environment_ambient_light(N) * 1.38 * environment_exposure() +
                      soi_key_light(N) * 0.86 * environment_exposure() + subsurface;
  float gloss = mix(0.020, 0.052, figure) * (1.0 + environment_wetness() * 1.8);
  float specular = pow(max(dot(N, H), 0.0), mix(22.0, 52.0, figure)) * gloss;
  float rim = pow(1.0 - max(dot(N, V), 0.0), 3.2) * mix(0.035, 0.075, figure);

  vec3 color = stone * illumination * cavity;
  color += sun * specular;
  color += sky * rim;
  color += stone * cavity * local_lighting(v_world_pos, normalize(v_normal));
  color = apply_directional_shadow(color, v_world_pos, v_normal);
  color /= 1.0 + max(color - vec3(0.95), vec3(0.0)) * 0.50;
  color = apply_visibility_memory(color, v_world_pos.xz);
  frag_color = vec4(color, 1.0);
}
