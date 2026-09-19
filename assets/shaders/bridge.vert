#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_tex_coord;

uniform mat4 u_mvp;
uniform mat4 u_model;

out vec3 v_normal;
out vec2 v_tex_coord;
out vec3 v_world_pos;
flat out int v_dressed_stone;

void main() {

  v_normal = normalize(mat3(transpose(inverse(u_model))) * a_normal);
  // Negative U marks a separate coping / arch stone; its joints are geometry.
  v_dressed_stone = a_tex_coord.x < 0.0 ? 1 : 0;
  v_tex_coord = vec2(a_tex_coord.x < 0.0 ? -a_tex_coord.x - 1.0 : a_tex_coord.x,
                      a_tex_coord.y);

  v_world_pos = vec3(u_model * vec4(a_position, 1.0));

  gl_Position = u_mvp * vec4(a_position, 1.0);
}
