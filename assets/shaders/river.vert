#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_tex_coord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float time;
uniform int u_water_surface_kind;

out vec2 tex_coord;
out vec3 world_pos;

void main() {

  vec3 pos = a_pos;

  float lake = float(u_water_surface_kind == 1);
  float speed = mix(1.0, 0.42, lake);
  float amplitude = mix(1.0, 0.58, lake);
  float wave1 = sin(a_pos.z * 0.5 + time * 2.0 * speed) * 0.04;
  float wave2 = sin(a_pos.x * 0.8 + time * 1.5 * speed) * 0.03;
  float wave3 = sin((a_pos.x + a_pos.z) * 0.3 + time * 2.5 * speed) * 0.02;

  float ripple1 = sin(a_pos.x * 2.0 + a_pos.z * 1.5 + time * 3.0) * 0.015;
  float ripple2 = cos(a_pos.z * 2.5 - a_pos.x * 1.2 + time * 2.2) * 0.012;

  pos.y += (wave1 + wave2 + wave3 + ripple1 + ripple2) * amplitude;

  world_pos = (model * vec4(pos, 1.0)).xyz;

  gl_Position = projection * view * model * vec4(pos, 1.0);
  tex_coord = a_tex_coord;
}
