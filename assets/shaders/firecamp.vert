#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_tex_coord;

layout(location = 3) in vec4 i_pos_intensity;
layout(location = 4) in vec4 i_radius_phase;

layout(std140) uniform FrameData { mat4 u_view_proj; };
uniform float u_time;
uniform float u_flicker_speed;
uniform float u_flicker_amount;
uniform vec3 u_camera_right;
uniform vec3 u_camera_forward;

out vec2 tex_coord;
out float intensity_val;
out float flame_phase;
out float flame_height;

void main() {
  vec3 camp_pos = i_pos_intensity.xyz;
  float intensity = i_pos_intensity.w;
  float phase = i_radius_phase.y;
  float radius = i_radius_phase.x;

  vec3 right_vec = normalize(vec3(u_camera_right.x, 0.0, u_camera_right.z));
  if (length(right_vec) < 1e-4)
    right_vec = vec3(1.0, 0.0, 0.0);
  vec3 forward_vec =
      normalize(vec3(u_camera_forward.x, 0.0, u_camera_forward.z));
  if (length(forward_vec) < 1e-4)
    forward_vec = normalize(vec3(-right_vec.z, 0.0, right_vec.x));
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

  float width_base = clamp(radius * 0.18 * intensity_scale, 0.55, 0.95);
  float width_scale = mix(width_base, width_base * 0.35, height_t);
  float height_scale = clamp(radius * 0.24 * intensity_scale, 0.55, 1.05);

  float flicker_offset =
      sin(u_time * u_flicker_speed + phase) * (u_flicker_amount * 0.55);
  float sway =
      sin(u_time * (u_flicker_speed * 1.05) + phase * 2.1 + height_t * 2.7);
  vec3 wobble_offset = horizontal_axis * (sway * u_flicker_amount * radius *
                                          (0.18 + height_t * 0.35));

  vec3 local_offset =
      horizontal_axis * (a_pos.x * width_scale) +
      up_vec * (a_pos.y * height_scale * (0.85 + height_t * 0.25));

  float taper = mix(0.0, width_base * 0.25, height_t * height_t);
  local_offset += horizontal_axis * (-a_pos.x * taper);

  float base_lift = radius * 0.02 + intensity * 0.04;
  vec3 pos = camp_pos + local_offset + wobble_offset +
             up_vec * (flicker_offset + base_lift);

  gl_Position = u_view_proj * vec4(pos, 1.0);
  tex_coord = a_tex_coord;
  intensity_val = intensity;
  flame_phase = phase;
  flame_height = height_t;
}
