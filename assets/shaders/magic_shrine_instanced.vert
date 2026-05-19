#version 330 core

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

float hash13(vec3 p) {
  p = fract(p * 0.1031);
  p += dot(p, p.yzx + 33.33);
  return fract((p.x + p.y) * p.z);
}

void main() {
  float scale = a_pos_scale.w;
  vec3 world_origin = a_pos_scale.xyz;
  float rotation = a_color_rot.a;

  float cos_r = cos(rotation);
  float sin_r = sin(rotation);
  mat2 rot = mat2(cos_r, -sin_r, sin_r, cos_r);

  vec3 local_pos = a_pos * scale;
  local_pos.xz = rot * local_pos.xz;

  v_world_pos = local_pos + world_origin;

  vec2 rotated_normal_xz = rot * a_normal.xz;
  v_normal = normalize(vec3(rotated_normal_xz.x, a_normal.y, rotated_normal_xz.y));

  v_color = a_color_rot.rgb;
  v_local_pos = a_pos * scale;

  v_seed = hash13(world_origin * 0.173 + vec3(rotation, scale, 0.91));

  gl_Position = u_view_proj * vec4(v_world_pos, 1.0);
}
