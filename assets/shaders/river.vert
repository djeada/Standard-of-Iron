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

  // River and lake surfaces share the same world-space wave field so their
  // junctions cannot shimmer apart. Surface kind only changes energy/speed:
  // broad lakes are calmer, flowing channels retain smaller quick ripples.
  float lake = float(u_water_surface_kind == 1);
  float speed = mix(1.0, 0.58, lake);
  float amplitude = mix(1.0, 0.68, lake);
  float wave1 = sin(a_pos.z * 0.34 + time * 1.20 * speed) * 0.012;
  float wave2 = sin(a_pos.x * 0.47 - time * 0.92 * speed) * 0.009;
  float wave3 = sin((a_pos.x + a_pos.z) * 0.21 + time * 0.74 * speed) * 0.007;
  float ripple1 = sin(a_pos.x * 1.65 + a_pos.z * 1.18 + time * 1.85 * speed) *
                  0.003;
  float ripple2 = cos(a_pos.z * 2.05 - a_pos.x * 0.92 + time * 1.55 * speed) *
                  0.002;

  pos.y += (wave1 + wave2 + wave3 + ripple1 + ripple2) * amplitude;

  world_pos = (model * vec4(pos, 1.0)).xyz;

  gl_Position = projection * view * model * vec4(pos, 1.0);
  tex_coord = a_tex_coord;
}
