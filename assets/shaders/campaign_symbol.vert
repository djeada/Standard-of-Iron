#version 330 core

layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_local_and_type;

uniform mat4 u_mvp;
uniform float u_z;

out vec2 v_local;
out float v_symbol_type;

void main() {
  vec3 world = vec3(1.0 - a_pos.x, u_z, a_pos.y);
  gl_Position = u_mvp * vec4(world, 1.0);

  v_local = vec2(a_local_and_type.x, 0.0);
  v_symbol_type = a_local_and_type.y;
}
