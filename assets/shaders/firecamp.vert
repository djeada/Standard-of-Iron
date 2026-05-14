#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_tex_coord;

layout(location = 3) in vec4 i_pos_intensity;
layout(location = 4) in vec4 i_radius_phase;

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

float hash(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise(vec2 p) {
  vec2 cell = floor(p);
  vec2 local = fract(p);
  vec2 smooth_local = local * local * (3.0 - 2.0 * local);

  float a = hash(cell);
  float b = hash(cell + vec2(1.0, 0.0));
  float c = hash(cell + vec2(0.0, 1.0));
  float d = hash(cell + vec2(1.0, 1.0));

  return mix(mix(a, b, smooth_local.x), mix(c, d, smooth_local.x), smooth_local.y);
}

void main() {
  vec3 camp_pos = i_pos_intensity.xyz;
  float intensity = i_pos_intensity.w;
  float phase = i_radius_phase.y;
  float radius = i_radius_phase.x;

  vec3 right_flat = vec3(u_camera_right.x, 0.0, u_camera_right.z);
  float right_len = length(right_flat);
  vec3 right_vec = right_len < 1e-4 ? vec3(1.0, 0.0, 0.0) : right_flat / right_len;

  vec3 forward_flat = vec3(u_camera_forward.x, 0.0, u_camera_forward.z);
  float forward_len = length(forward_flat);
  vec3 forward_vec = forward_len < 1e-4
                         ? normalize(vec3(-right_vec.z, 0.0, right_vec.x))
                         : forward_flat / forward_len;
  vec3 up_vec = vec3(0.0, 1.0, 0.0);

  float plane_id = floor(a_pos.z + 0.5);
  float angle = plane_id * 2.0943951;
  float c = cos(angle);
  float s = sin(angle);
  vec3 horizontal_axis = normalize(right_vec * c + forward_vec * s);
  if (length(horizontal_axis) < 1e-4)
    horizontal_axis = right_vec;

  float intensity_scale = clamp(intensity, 0.65, 1.4);
  float height_t = clamp(a_tex_coord.y, 0.0, 1.0);
  float centered_x = a_tex_coord.x * 2.0 - 1.0;
  float tip_bias = pow(height_t, 1.35);
  float pulse = 0.94 + 0.12 * sin(u_time * (u_flicker_speed * 0.62) + phase * 1.7) +
                0.05 * sin(u_time * (u_flicker_speed * 1.8) + phase * 2.8);
  float curl_noise = noise(vec2(centered_x * 1.6 + phase * 0.12,
                                height_t * 3.7 - u_time * (u_flicker_speed * 0.34)));
  float gust_noise = noise(vec2(phase * 0.21, u_time * 0.22 + plane_id * 1.73));

  float width_base = clamp(radius * 0.18 * intensity_scale, 0.55, 0.95);
  float width_scale = mix(width_base * pulse,
                          width_base * (0.18 + 0.12 * gust_noise + 0.08 * curl_noise),
                          tip_bias);
  float height_scale =
      clamp(radius * 0.24 * intensity_scale, 0.55, 1.08) * (0.96 + 0.12 * pulse);

  float flicker_offset = (gust_noise - 0.5) * (u_flicker_amount * 0.7);
  float sway = sin(u_time * (u_flicker_speed * 1.08) + phase * 2.1 + height_t * 5.3 +
                   centered_x * 4.2);
  float lean = (sway * 0.55 + (curl_noise - 0.5) * 1.2 + (gust_noise - 0.5) * 1.1) *
               u_flicker_amount * radius * (0.12 + tip_bias * 1.15);
  float swirl = sin(u_time * (u_flicker_speed * 1.45) + phase * 3.1 + centered_x * 6.2);
  vec3 wobble_offset =
      horizontal_axis * ((sway * 0.32 + (gust_noise - 0.5) * 0.75) * u_flicker_amount *
                         radius * (0.08 + height_t * 0.42));
  vec3 lean_axis = normalize(
      mix(horizontal_axis, forward_vec, 0.28 * sin(phase * 1.11 + u_time * 0.41)));

  vec3 local_offset = horizontal_axis * (centered_x * width_scale) +
                      up_vec * (a_pos.y * height_scale * (0.85 + tip_bias * 0.28));

  float taper = mix(0.0, width_base * 0.3, tip_bias);
  local_offset += horizontal_axis * (-centered_x * taper);
  local_offset += lean_axis * lean;
  local_offset +=
      forward_vec * (swirl * u_flicker_amount * radius * (0.03 + tip_bias * 0.22));

  float base_lift = radius * 0.02 + intensity * 0.04;
  float vertical_lift =
      (0.3 + 0.7 * curl_noise) * u_flicker_amount * radius * (0.04 + height_t * 0.36);
  vec3 pos = camp_pos + local_offset + wobble_offset +
             up_vec * (flicker_offset + base_lift + vertical_lift);

  gl_Position = u_view_proj * vec4(pos, 1.0);
  tex_coord = a_tex_coord;
  intensity_val = intensity;
  flame_phase = phase;
  flame_height = height_t;
}
