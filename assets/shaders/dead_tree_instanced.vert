#version 330 core
#include "noise.glsl"

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_pos_scale;
layout(location = 3) in vec4 a_color_rot;

layout(std140) uniform FrameData {
  mat4 u_view_proj;
};

out vec3 v_world_pos;
out vec3 v_normal;
out vec3 v_color;
out vec3 v_local_pos;
out vec3 v_local_normal;

const float k_dead_tree_twist = 0.55;
const float k_dead_tree_lean = 0.16;

void main() {
  float scale = a_pos_scale.w;
  vec3 world_pos = a_pos_scale.xyz;
  float rotation = a_color_rot.a;

  float cos_r = cos(rotation);
  float sin_r = sin(rotation);
  mat2 rot = mat2(cos_r, -sin_r, sin_r, cos_r);

  float seed = soi_hash13_1c8396(world_pos * 0.173 + vec3(rotation, scale, 0.61));
  float seed_b = fract(seed * 7.31 + 0.19);
  float seed_c = fract(seed * 13.17 + 0.53);
  vec3 stretch =
      vec3(mix(0.80, 1.24, seed_b), mix(0.84, 1.20, seed), mix(0.80, 1.24, seed_c));
  vec3 shaped = a_pos * stretch;
  float twist = (seed_c - 0.5) * k_dead_tree_twist * shaped.y;
  float twist_c = cos(twist);
  float twist_s = sin(twist);
  shaped.xz = mat2(twist_c, -twist_s, twist_s, twist_c) * shaped.xz;
  float lean_angle = seed_b * 6.2831853;
  shaped.xz +=
      vec2(cos(lean_angle), sin(lean_angle)) * shaped.y * (k_dead_tree_lean * seed);

  vec3 local_pos = shaped * scale;
  vec2 rotated_xz = rot * local_pos.xz;
  local_pos = vec3(rotated_xz.x, local_pos.y, rotated_xz.y);

  v_world_pos = local_pos + world_pos;

  vec3 shaped_normal = normalize(a_normal / stretch);
  vec2 rotated_normal_xz = rot * shaped_normal.xz;
  v_normal = normalize(vec3(rotated_normal_xz.x, shaped_normal.y, rotated_normal_xz.y));

  v_color = a_color_rot.rgb;
  v_local_pos = a_pos;
  v_local_normal = normalize(a_normal);

  gl_Position = u_view_proj * vec4(v_world_pos, 1.0);
}
