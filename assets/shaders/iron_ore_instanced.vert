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
flat out float v_seed;

const float k_ore_cant = 0.22;

void main() {
  float scale = a_pos_scale.w;
  vec3 world_origin = a_pos_scale.xyz;
  float rotation = a_color_rot.a;

  float cos_r = cos(rotation);
  float sin_r = sin(rotation);
  mat2 rot = mat2(cos_r, -sin_r, sin_r, cos_r);

  float shape_seed =
      soi_hash13_1c8396(world_origin * 0.291 + vec3(scale, rotation, 0.83));
  float shape_seed_b = fract(shape_seed * 9.13 + 0.27);
  vec3 stretch = vec3(mix(0.80, 1.30, shape_seed),
                      mix(0.74, 1.22, shape_seed_b),
                      mix(0.80, 1.30, fract(shape_seed * 5.71 + 0.61)));
  vec3 shaped = a_pos * stretch;
  shaped.xz += vec2(shape_seed_b - 0.5, shape_seed - 0.5) * shaped.y * k_ore_cant;

  vec3 local_pos = shaped * scale;
  local_pos.xz = rot * local_pos.xz;

  v_world_pos = local_pos + world_origin;

  vec3 shaped_normal = normalize(a_normal / stretch);
  vec2 rotated_normal_xz = rot * shaped_normal.xz;
  v_normal = normalize(vec3(rotated_normal_xz.x, shaped_normal.y, rotated_normal_xz.y));

  v_color = a_color_rot.rgb;

  v_local_pos = a_pos;

  v_seed = soi_hash13_1c8396(world_origin * 0.173 + vec3(rotation, scale, 0.37));

  gl_Position = u_view_proj * vec4(v_world_pos, 1.0);
}
