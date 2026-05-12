#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_tex_coord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float time;

out vec2 tex_coord;
out vec3 world_pos;
out vec3 v_normal;

void main() {
  vec3 pos = a_pos;

  float sway = sin(a_pos.x * 0.3 + time * 0.5) * 0.005;
  pos.y += sway;

  world_pos = (model * vec4(pos, 1.0)).xyz;
  v_normal = normalize(mat3(transpose(inverse(model))) * a_normal);

  gl_Position = projection * view * model * vec4(pos, 1.0);
  tex_coord = a_tex_coord;
}
