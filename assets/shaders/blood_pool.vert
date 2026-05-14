#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 2) in vec2 a_tex_coord;

uniform mat4 u_mvp;
uniform float u_rotation;
uniform float u_aspect_ratio;
out vec2 v_uv;

void main() {
  v_uv = a_tex_coord;
  float s = sin(u_rotation);
  float c = cos(u_rotation);
  vec2 local = vec2(a_position.x * u_aspect_ratio, a_position.z);
  vec2 rotated = vec2(local.x * c - local.y * s, local.x * s + local.y * c);
  gl_Position = u_mvp * vec4(rotated.x, a_position.y, rotated.y, 1.0);
}
