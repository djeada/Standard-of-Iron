#version 330 core
#include "noise.glsl"
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_tex_coord;

layout(location = 3) in vec4 i_pos_intensity;
layout(location = 4) in vec4 i_radius_phase;
layout(location = 5) in vec4 i_ground;

layout(std140) uniform FrameData {
  mat4 u_view_proj;
};
uniform float u_time;
uniform float u_flicker_speed;
uniform float u_flicker_amount;
uniform vec3 u_camera_right;
uniform vec3 u_camera_forward;

out vec2 tex_coord;
out float intensity_val;
out float flame_phase;
out float flame_height;
flat out int v_part;

const int k_part_flame = 0;
const int k_part_hearth = 1;
const int k_part_smoke = 2;

void main() {
  vec3 camp_pos = i_pos_intensity.xyz;
  float intensity = clamp(i_pos_intensity.w, 0.6, 1.6);
  float phase = i_radius_phase.y;
  vec2 ground_slope = i_ground.xy;
  float hearth_radius = max(i_ground.z, 0.2);

  vec3 right_flat = vec3(u_camera_right.x, 0.0, u_camera_right.z);
  float right_len = length(right_flat);
  vec3 right_vec = right_len < 1e-4 ? vec3(1.0, 0.0, 0.0) : right_flat / right_len;
  vec3 forward_flat = vec3(u_camera_forward.x, 0.0, u_camera_forward.z);
  float forward_len = length(forward_flat);
  vec3 forward_vec = forward_len < 1e-4
                         ? normalize(vec3(-right_vec.z, 0.0, right_vec.x))
                         : forward_flat / forward_len;
  vec3 up_vec = vec3(0.0, 1.0, 0.0);

  float part_id = floor(a_pos.z + 0.5);
  float intensity_scale = clamp(intensity, 0.7, 1.35);
  float flame_tall = hearth_radius * 1.75 * intensity_scale;
  float flame_wide = hearth_radius * 0.66 * mix(0.9, 1.08, intensity_scale - 0.7);

  vec3 wind = normalize(vec3(0.82, 0.0, 0.57));
  float gust = soi_noise_95f501(vec2(phase * 0.37, u_time * 0.31));
  vec3 pos;

  if (part_id > 2.5 && part_id < 3.5) {
    vec2 disc = a_tex_coord;
    vec2 offset = disc * hearth_radius * 0.86;
    float slope = length(ground_slope);
    float lift = 0.022 + 0.06 * min(slope, 1.5) + i_ground.w;
    pos = camp_pos + vec3(offset.x, dot(ground_slope, offset) + lift, offset.y);
    v_part = k_part_hearth;
    flame_height = 0.0;
  } else if (part_id > 3.5) {
    float h = clamp(a_tex_coord.y, 0.0, 1.0);
    float centered_x = a_tex_coord.x * 2.0 - 1.0;
    float rise = flame_tall * 0.55 + h * flame_tall * 2.3;
    float width = flame_wide * mix(0.55, 1.75, h);
    float drift = h * h * flame_tall * (0.55 + 0.35 * gust);
    pos = camp_pos + up_vec * rise + right_vec * (centered_x * width) + wind * drift;
    v_part = k_part_smoke;
    flame_height = h;
  } else {
    float angle = part_id * 1.0471976;
    vec3 horizontal_axis = normalize(right_vec * cos(angle) + forward_vec * sin(angle));

    float h = clamp(a_tex_coord.y, 0.0, 1.0);
    float centered_x = a_tex_coord.x * 2.0 - 1.0;
    float pulse = 0.95 + 0.07 * sin(u_time * (u_flicker_speed * 0.62) + phase * 1.7) +
                  0.04 * sin(u_time * (u_flicker_speed * 1.8) + phase * 2.8);
    float sway = sin(u_time * (u_flicker_speed * 0.9) + phase * 2.1 + h * 3.1) * 0.6 +
                 (gust - 0.5) * 0.8;
    vec3 lean = (wind * 0.6 + horizontal_axis * 0.4) * sway * u_flicker_amount *
                flame_tall * h * h * 2.4;

    pos = camp_pos + horizontal_axis * (centered_x * flame_wide) +
          up_vec * (h * flame_tall * pulse + hearth_radius * 0.04) + lean;
    v_part = k_part_flame;
    flame_height = h;
  }

  gl_Position = u_view_proj * vec4(pos, 1.0);
  tex_coord = a_tex_coord;
  intensity_val = intensity;
  flame_phase = phase;
}
