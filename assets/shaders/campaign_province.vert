#version 330 core

layout(location = 0) in vec2 a_pos;

uniform mat4 u_mvp;
uniform float u_z;

out vec2 v_uv;
out vec2 v_world_pos;

void main() {
  vec3 world = vec3(1.0 - a_pos.x, u_z, a_pos.y);
  gl_Position = u_mvp * vec4(world, 1.0);

  v_uv = a_pos;
  v_world_pos = a_pos;
}
