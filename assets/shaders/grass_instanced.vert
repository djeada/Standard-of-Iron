#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_pos_height;
layout(location = 3) in vec4 a_color_width;
layout(location = 4) in vec4 a_sway_params;

layout(std140) uniform FrameData { mat4 u_view_proj; };
uniform float u_time;
uniform float u_wind_strength;
uniform float u_wind_speed;
uniform vec3 u_soil_color;
uniform vec3 u_light_dir;

out vec3 v_color;
out float v_alpha;

void main() {
  vec3 base_pos = a_pos_height.xyz;
  float blade_height = a_pos_height.w;

  vec3 blade_color = a_color_width.xyz;
  float blade_width = a_color_width.w;

  float sway_strength = a_sway_params.x * u_wind_strength;
  float sway_speed = a_sway_params.y * u_wind_speed;
  float sway_phase = a_sway_params.z;
  float orientation = a_sway_params.w;

  float tip = clamp(a_uv.y, 0.0, 1.0);
  float sway = sin(u_time * sway_speed + sway_phase) * sway_strength;
  float bend = smoothstep(0.0, 1.0, tip);
  float sway_offset = sway * bend;

  vec3 local_pos = vec3(a_position.x * blade_width + sway_offset,
                        a_position.y * blade_height, 0.0);

  float sin_o = sin(orientation);
  float cos_o = cos(orientation);
  vec3 rotated = vec3(local_pos.x * cos_o - local_pos.z * sin_o, local_pos.y,
                      local_pos.x * sin_o + local_pos.z * cos_o);

  vec3 world_pos = base_pos + rotated;

  vec3 light_dir = normalize(u_light_dir);
  vec3 normal = normalize(vec3(sin_o, 1.6, cos_o));
  float light_term = clamp(dot(normal, light_dir), 0.0, 1.0);
  float tip_highlight = mix(0.7, 1.0, tip);
  vec3 root_tint = mix(u_soil_color * 0.45, blade_color, 0.52);
  vec3 soil_blend = mix(root_tint, blade_color, smoothstep(0.0, 0.72, tip));
  v_color = soil_blend * (0.7 + 0.3 * light_term) * tip_highlight;

  float edge_fade = 1.0 - smoothstep(0.35, 0.5, abs(a_uv.x - 0.5));
  v_alpha = clamp(0.35 + 0.45 * tip, 0.25, 0.85) * edge_fade;

  gl_Position = u_view_proj * vec4(world_pos, 1.0);
}
