#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_tex_coord;

layout(location = 3) in vec3 i_center;
layout(location = 4) in float i_size;
layout(location = 5) in vec3 i_color;
layout(location = 6) in float i_alpha;

layout(std140) uniform FrameData { mat4 u_view_proj; };
uniform float u_time;

out vec3 v_world_pos;
out vec3 v_color;
out float v_alpha;
out vec2 v_local_coord;
out float v_seed;

float hash11(float x) { return fract(sin(x * 91.345) * 43758.5453); }

float hash21(vec2 p) {
  p = fract(p * vec2(123.34, 456.21));
  p += dot(p, p + 78.233);
  return fract(p.x * p.y);
}

void main() {
  float seed = hash21(i_center.xz * 0.137);
  float ang = seed * 6.2831853;
  float c = cos(ang);
  float s = sin(ang);
  float jitter = 0.92 + hash11(seed + 3.17) * 0.18;
  vec2 local = vec2(a_position.x, a_position.z) * (i_size * jitter);
  vec2 rot = vec2(c * local.x - s * local.y, s * local.x + c * local.y);
  vec3 world_pos = vec3(i_center.x + rot.x,
                       i_center.y + a_position.y +
                           sin(dot(i_center.xz, vec2(0.041, 0.033)) +
                               u_time * 0.10 + seed * 5.0) *
                               i_size * 0.012,
                       i_center.z + rot.y);

  v_world_pos = world_pos;
  v_color = i_color;
  v_alpha = i_alpha;
  v_local_coord = rot / max(i_size * jitter, 0.001);
  v_seed = seed;

  gl_Position = u_view_proj * vec4(world_pos, 1.0);
}
