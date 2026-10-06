#version 330 core

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_tex_coord;
layout(location = 2) in vec3 a_normal;
layout(location = 3) in vec4 a_pos_scale;
layout(location = 4) in vec4 a_color_sway;
layout(location = 5) in vec4 a_rotation;

layout(std140) uniform FrameData {
  mat4 u_view_proj;
};
uniform float u_time;
uniform float u_wind_strength;
uniform float u_wind_speed;

out vec3 v_world_pos;
out vec3 v_normal;
out vec3 v_color;
out vec3 v_tex_coord;
out vec3 v_local_pos;
out float v_frond_reach;
out float v_leaf_seed;
out float v_bark_seed;

const vec3 k_crown = vec3(0.073, 1.16, 0.028);
const float k_frond_length = 0.78;

void main() {
  const float TWO_PI = 6.2831853;

  float scale = a_pos_scale.w;
  vec3 world_pos = a_pos_scale.xyz;
  float sway_phase = a_color_sway.a;
  float rotation = a_rotation.x;
  float silhouette_seed = a_rotation.y;
  float leaf_seed = a_rotation.z;
  float bark_seed = a_rotation.w;

  vec3 model_pos = a_pos;
  float frond = step(0.75, a_tex_coord.z);
  vec2 from_crown = a_pos.xz - k_crown.xz;
  float reach = frond * clamp(length(from_crown) / k_frond_length, 0.0, 1.0);

  float height_norm = clamp(a_pos.y / k_crown.y, 0.0, 1.0);
  float lean_angle = (silhouette_seed - 0.5) * 0.26;
  float lean_yaw = bark_seed * TWO_PI;
  vec2 lean = vec2(cos(lean_yaw), sin(lean_yaw)) * lean_angle;
  model_pos.xz += lean * height_norm * height_norm;

  float droop = mix(0.82, 1.18, fract(leaf_seed * 5.71 + a_tex_coord.x * 3.3));
  model_pos.y -= reach * reach * (droop - 1.0) * 0.30;

  vec3 local_pos = model_pos * scale;

  float wind_time = u_time * u_wind_speed * 0.4;
  float bend = height_norm * height_norm;
  float gust = 0.60 + 0.40 * sin(wind_time * 0.23 + sway_phase * 0.41);
  float sway = sin(wind_time + sway_phase) * u_wind_strength * gust * bend;
  float sway2 = sin(wind_time * 1.7 + sway_phase * 2.3) * u_wind_strength * 0.38 * bend;
  vec2 wind_dir = normalize(vec2(0.78, 0.62));
  vec2 cross_dir = vec2(-wind_dir.y, wind_dir.x);
  vec2 wind_offset = (wind_dir * sway + cross_dir * sway2) * 0.10 * scale;

  float frond_flutter =
      sin(wind_time * 3.4 + a_tex_coord.x * TWO_PI * 3.0 + leaf_seed * TWO_PI) *
      sin(wind_time * 1.3 + sway_phase * 0.83 + a_tex_coord.x * 7.0);
  float flutter_flex = reach * reach;
  wind_offset +=
      cross_dir * frond_flutter * flutter_flex * gust * u_wind_strength * 0.030 * scale;
  local_pos.y += frond_flutter * flutter_flex * u_wind_strength * 0.022 * scale;

  float cos_r = cos(rotation);
  float sin_r = sin(rotation);
  mat2 rot = mat2(cos_r, -sin_r, sin_r, cos_r);

  vec2 rotated_xz = rot * local_pos.xz + wind_offset;
  local_pos = vec3(rotated_xz.x, local_pos.y, rotated_xz.y);

  vec2 rotated_normal_xz = rot * a_normal.xz;
  vec3 final_normal =
      normalize(vec3(rotated_normal_xz.x, a_normal.y, rotated_normal_xz.y));

  v_world_pos = local_pos + world_pos;
  v_normal = final_normal;
  v_color = a_color_sway.rgb;
  v_tex_coord = a_tex_coord;
  v_local_pos = model_pos;
  v_frond_reach = reach;
  v_leaf_seed = leaf_seed;
  v_bark_seed = bark_seed;

  gl_Position = u_view_proj * vec4(v_world_pos, 1.0);
}
