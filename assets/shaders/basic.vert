#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_tex_coord;

uniform mat4 u_mvp;
uniform mat4 u_model;
layout(std140) uniform FrameData { mat4 u_view_proj; };

out vec3 v_normal;
out vec2 v_tex_coord;
out vec3 v_world_pos;

void main() {
  vec4 worldPos4 = u_model * vec4(a_position, 1.0);
  gl_Position = u_view_proj * worldPos4;
  v_normal = mat3(u_model) * a_normal;
  v_tex_coord = a_tex_coord;
  v_world_pos = worldPos4.xyz;
}