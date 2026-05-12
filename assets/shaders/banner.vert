#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_tex_coord;

uniform mat4 u_mvp;
uniform mat4 u_model;
uniform float u_time;
uniform float u_wind_strength;

out vec3 v_normal;
out vec2 v_tex_coord;
out float v_wave_offset;
out float v_cloth_depth;

void main() {
  vec3 pos = a_position;

  float horizontal_pos = clamp(a_tex_coord.x, 0.0, 1.0);
  float vertical_pos = clamp(1.0 - a_tex_coord.y, 0.0, 1.0);
  float free_edge = horizontal_pos * horizontal_pos;
  float center_bias = 1.0 - abs(vertical_pos * 2.0 - 1.0);

  float wave_phase = u_time * 2.3 + horizontal_pos * 4.8 - vertical_pos * 1.6;
  float z_offset =
      sin(wave_phase) * u_wind_strength * free_edge * (0.18 + center_bias * 0.08);

  float ripple_phase = u_time * 5.1 + horizontal_pos * 10.5 + vertical_pos * 6.0;
  float ripple = sin(ripple_phase) * u_wind_strength * free_edge * 0.045;

  float curl_phase = u_time * 7.8 + vertical_pos * 3.5 + horizontal_pos * 13.0;
  float trailing_curl =
      sin(curl_phase) * u_wind_strength * free_edge * free_edge * 0.03;

  float sway_phase = u_time * 1.6 + horizontal_pos * 1.4 + vertical_pos * 2.4;
  float y_offset = sin(sway_phase) * u_wind_strength * free_edge * 0.04;
  float sag = free_edge * (0.014 + (1.0 - vertical_pos) * 0.02) * u_wind_strength;

  pos.z += z_offset + ripple + trailing_curl;
  pos.y += y_offset - sag;
  pos.x -= free_edge * u_wind_strength * 0.035;

  vec3 normal = a_normal;
  normal.z += cos(wave_phase) * u_wind_strength * free_edge * 0.45;
  normal.x -= sin(ripple_phase) * u_wind_strength * free_edge * 0.12;
  normal = normalize(normal);

  v_normal = mat3(transpose(inverse(u_model))) * normal;
  v_tex_coord = a_tex_coord;
  v_wave_offset = z_offset + ripple;
  v_cloth_depth = horizontal_pos;

  gl_Position = u_mvp * vec4(pos, 1.0);
}
