#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_tex_coord;

uniform mat4 u_mvp;
uniform float u_time;

out vec3 v_normal;
out float v_layer;
out float v_radial;
out float v_phase;

void main() {
  v_normal = a_normal;
  v_layer = a_tex_coord.x;
  v_radial = a_tex_coord.y;
  v_phase = 0.0;

  float pulse = 1.0 + 0.015 * sin(u_time * 2.0);
  vec3 pos = vec3(a_position.xy * pulse, a_position.z);

  gl_Position = u_mvp * vec4(pos, 1.0);
}
