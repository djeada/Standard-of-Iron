#version 330 core

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_tex_coord;
layout(location = 2) in vec3 a_normal;
layout(location = 3) in vec4 a_pos_scale;
layout(location = 4) in vec4 a_color_sway;
layout(location = 5) in vec4 a_rotation;

#include "frame_data.glsl"
uniform float u_time;
uniform float u_wind_strength;
uniform float u_wind_speed;

out vec3 v_world_pos;
out vec3 v_normal;
out vec3 v_color;
out vec2 v_tex_coord;
out float v_foliage_mask;
out float v_needle_seed;
out float v_bark_seed;
out vec3 v_local_pos;
out float v_bough;

void main() {
  const float TWO_PI = 6.2831853;

  float scale = a_pos_scale.w;
  vec3 world_pos = a_pos_scale.xyz;
  float sway_phase = a_color_sway.a;
  float rotation = a_rotation.x;
  float silhouette_seed = a_rotation.y;
  float needle_seed = a_rotation.z;
  float bark_seed = a_rotation.w;

  vec3 model_pos = a_pos;
  float source_radius = length(a_pos.xz);

  float foliage_mask = smoothstep(0.34, 0.42, a_tex_coord.y);
  float tip_mask = smoothstep(0.88, 1.02, a_tex_coord.y);
  float angle = a_tex_coord.x * TWO_PI;

  float aspect_w = mix(0.78, 1.22, needle_seed);
  float aspect_h = mix(1.08, 0.92, needle_seed) *
                   mix(0.86, 1.16, fract(bark_seed * 7.31 + silhouette_seed * 3.17));
#ifndef SOI_CYPRESS
  aspect_w *= mix(0.76, 1.12, silhouette_seed);
#endif
  model_pos.xz *= mix(1.0, aspect_w, foliage_mask);
  model_pos.y *= mix(1.0, aspect_h, foliage_mask);

  // Change the azimuth gradually up the crown. Branch whorls keep their
  // individual shapes while neighbouring instances expose different gaps.
  float twist = (silhouette_seed - 0.5) * a_pos.y * 0.85;
  mat2 crown_turn = mat2(cos(twist), -sin(twist), sin(twist), cos(twist));
  model_pos.xz = crown_turn * model_pos.xz;

  float lean_norm = clamp(a_pos.y / 1.30, 0.0, 1.0);
  float lean_angle = (silhouette_seed - 0.5) * 0.18;
  float lean_yaw = bark_seed * TWO_PI;
  model_pos.x += cos(lean_yaw) * lean_angle * lean_norm;
  model_pos.z += sin(lean_yaw) * lean_angle * lean_norm;

  float height_factor = clamp(model_pos.y, 0.0, 1.1);
  vec3 local_pos = model_pos * scale;

  float wind_time = u_time * u_wind_speed;
  float bend = height_factor * height_factor;
  float gust = 0.62 + 0.38 * sin(wind_time * 0.19 + sway_phase * 0.37);
  float sway = sin(wind_time * 0.48 + sway_phase) * u_wind_strength * gust * bend;
  float secondary =
      sin(wind_time * 0.73 + sway_phase * 1.71 + 1.4) * u_wind_strength * 0.34 * bend;

  float sway_influence = mix(0.06, 0.20, foliage_mask) * scale;
  vec2 wind_dir = normalize(vec2(0.78, 0.62));
  vec2 cross_dir = vec2(-wind_dir.y, wind_dir.x);
  vec2 wind_offset = (wind_dir * sway + cross_dir * secondary) * sway_influence;
  float branch_flex =
      foliage_mask * smoothstep(0.16, 0.48, source_radius) * mix(0.45, 1.0, tip_mask);
  float branch_flutter =
      sin(wind_time * 2.45 + angle * 3.0 + sway_phase * 1.37 + needle_seed * TWO_PI) *
      sin(wind_time * 0.91 + angle * 1.7 + silhouette_seed * TWO_PI);
  wind_offset +=
      cross_dir * branch_flutter * branch_flex * gust * u_wind_strength * 0.018 * scale;
  local_pos.y -= abs(sway) * 0.012 * foliage_mask * scale;

  vec3 local_normal =
      a_normal / mix(vec3(1.0), vec3(aspect_w, aspect_h, aspect_w), foliage_mask);
  local_normal.xz = crown_turn * local_normal.xz;
  local_normal = normalize(local_normal);

  float cos_r = cos(rotation);
  float sin_r = sin(rotation);
  mat2 rot = mat2(cos_r, -sin_r, sin_r, cos_r);

  vec2 rotated_xz = rot * local_pos.xz + wind_offset;
  local_pos = vec3(rotated_xz.x, local_pos.y, rotated_xz.y);

  vec2 rotated_normal_xz = rot * local_normal.xz;
  vec3 final_normal =
      normalize(vec3(rotated_normal_xz.x, local_normal.y, rotated_normal_xz.y));

  v_world_pos = local_pos + world_pos;
  v_normal = final_normal;
  v_color = a_color_sway.rgb;
  v_tex_coord = a_tex_coord.xy;
  v_bough = a_tex_coord.z;
  v_foliage_mask = foliage_mask;
  v_needle_seed = needle_seed;
  v_bark_seed = bark_seed;
  v_local_pos = model_pos;

  gl_Position = u_view_proj * vec4(v_world_pos, 1.0);
}
