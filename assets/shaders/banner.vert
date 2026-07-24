#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_tex_coord;

uniform mat4 u_mvp;
uniform mat4 u_model;
uniform float u_time;
uniform float u_wind_strength;

out vec3 v_world_pos;
out vec3 v_normal;
out vec2 v_tex_coord;
out float v_billow;
out float v_cloth_depth;
out float v_banner_seed;

float hash12(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {
  vec3 pos = a_position;
  float u = clamp(a_tex_coord.x, 0.0, 1.0);
  float v = clamp(a_tex_coord.y, 0.0, 1.0);

  vec2 world_anchor = vec2(u_model[3].x, u_model[3].z);
  float banner_seed = hash12(floor(world_anchor * 3.7) + vec2(7.1, 3.9));
  float spatial_phase = banner_seed * 6.2831853;
  float wind = clamp(u_wind_strength, 0.0, 1.35);

  float attached = smoothstep(0.015, 0.16, u);
  float free_edge = attached * pow(u, 0.72);
  float free_edge_sq = free_edge * free_edge;
  float gust = 0.66 + 0.34 * sin(u_time * 0.43 + spatial_phase * 0.73);
  gust += (hash12(vec2(floor(u_time * 0.37 + banner_seed * 11.0), banner_seed)) - 0.5) *
          0.16;

  float primary_phase = u_time * 1.85 + u * 5.4 + v * 1.25 + spatial_phase;
  float cross_phase = u_time * 3.65 + u * 10.8 - v * 4.1 + spatial_phase * 1.7;
  float flutter_phase = u_time * 7.4 + u * 18.0 + v * 7.0 + spatial_phase * 2.3;

  float primary_amp = wind * gust * free_edge * mix(0.055, 0.145, u);
  float cross_amp = wind * free_edge * (0.020 + free_edge_sq * 0.030);
  float flutter_amp = wind * free_edge_sq * 0.014;
  float billow = sin(primary_phase) * primary_amp + sin(cross_phase) * cross_amp +
                 sin(flutter_phase) * flutter_amp;

  float lower_weight = smoothstep(0.38, 1.0, v);
  float sag = wind * free_edge * (0.009 + lower_weight * 0.018);
  float vertical_flutter = sin(cross_phase + 1.25) * wind * free_edge_sq * 0.012;

  pos.y += billow;
  pos.z += sag + vertical_flutter;
  pos.x -= wind * free_edge_sq * (0.012 + abs(billow) * 0.050);

  float d_depth_dx = cos(primary_phase) * primary_amp * 5.4 +
                     cos(cross_phase) * cross_amp * 10.8 +
                     cos(flutter_phase) * flutter_amp * 18.0;
  float d_depth_dz = cos(primary_phase) * primary_amp * 1.25 -
                     cos(cross_phase) * cross_amp * 4.1 +
                     cos(flutter_phase) * flutter_amp * 7.0;
  vec3 cloth_normal = normalize(vec3(-d_depth_dx, 1.0, -d_depth_dz));

  v_world_pos = vec3(u_model * vec4(pos, 1.0));
  v_normal = normalize(mat3(transpose(inverse(u_model))) * cloth_normal);
  v_tex_coord = a_tex_coord;
  v_billow = billow;
  v_cloth_depth = u;
  v_banner_seed = banner_seed;

  gl_Position = u_mvp * vec4(pos, 1.0);
}
