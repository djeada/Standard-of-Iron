#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_tex_coord;

uniform mat4 u_mvp;
uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

out vec3 v_normal;
out vec2 v_tex_coord;
out vec3 v_world_pos;

void main() {
  // Transform normal to world space
  v_normal = normalize(mat3(transpose(inverse(u_model))) * a_normal);
  v_tex_coord = a_tex_coord;

  // World position for lighting and procedural texturing
  v_world_pos = vec3(u_model * vec4(a_position, 1.0));

  gl_Position = u_mvp * vec4(a_position, 1.0);
}
