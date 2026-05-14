#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_tex_coord;

uniform mat4 u_model;
layout(std140) uniform FrameData {
  mat4 u_view_proj;
};
uniform float u_time;

out vec2 v_tex_coord;
out vec3 v_world_pos;
out vec3 v_world_normal;
out float v_sheet_drift;

float hash(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float a = hash(i);
  float b = hash(i + vec2(1.0, 0.0));
  float c = hash(i + vec2(0.0, 1.0));
  float d = hash(i + vec2(1.0, 1.0));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main() {
  vec3 world_normal = normalize(mat3(u_model) * a_normal);
  vec3 world_pos = (u_model * vec4(a_position, 1.0)).xyz;

  float base_noise = noise(world_pos.xz * 0.06 + vec2(a_tex_coord.y * 1.6, 0.0));
  float upper_noise =
      noise(world_pos.xz * 0.11 + vec2(0.0, a_tex_coord.y * 2.3) - u_time * 0.015);
  float drift_phase = u_time * 0.55 + world_pos.y * 0.22 + base_noise * 4.0;

  float lateral_drift = sin(drift_phase) * (0.18 + 0.22 * upper_noise) * a_tex_coord.y;
  float breathing = sin(u_time * 0.32 + world_pos.x * 0.04 + world_pos.z * 0.05) *
                    0.12 * (0.35 + 0.65 * a_tex_coord.y);
  float vertical_curl =
      sin(u_time * 0.40 + world_pos.x * 0.05 + base_noise * 6.0) * 0.18;

  world_pos += world_normal * (lateral_drift + breathing);
  world_pos.y += vertical_curl * (0.18 + 0.82 * a_tex_coord.y);

  v_tex_coord = a_tex_coord;
  v_world_pos = world_pos;
  v_world_normal = world_normal;
  v_sheet_drift = lateral_drift + breathing;

  gl_Position = u_view_proj * vec4(world_pos, 1.0);
}
