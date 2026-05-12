#version 330 core

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_pos_scale;
layout(location = 3) in vec4 a_color_rot;

layout(std140) uniform FrameData { mat4 u_view_proj; };

out vec3 v_world_pos;
out vec3 v_normal;
out vec3 v_color;

void main() {
  float scale = a_pos_scale.w;
  vec3 worldPos = a_pos_scale.xyz;
  float rotation = a_color_rot.a;

  float cosR = cos(rotation);
  float sinR = sin(rotation);
  mat2 rot = mat2(cosR, -sinR, sinR, cosR);

  vec3 localPos = a_pos * scale;
  vec2 rotatedXZ = rot * localPos.xz;
  localPos = vec3(rotatedXZ.x, localPos.y, rotatedXZ.y);

  v_world_pos = localPos + worldPos;

  vec2 rotatedNormalXZ = rot * a_normal.xz;
  v_normal = normalize(vec3(rotatedNormalXZ.x, a_normal.y, rotatedNormalXZ.y));

  v_color = a_color_rot.rgb;

  gl_Position = u_view_proj * vec4(v_world_pos, 1.0);
}
