#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texcoord;

uniform mat4 u_mvp;
uniform mat4 u_model;
uniform float u_time;
uniform vec3 u_center;
uniform float u_radius;
uniform float u_intensity;
uniform int u_effect_type;

out vec3 v_world_pos;
out vec3 v_normal;
out vec2 v_texcoord;
out float v_intensity;
out float v_alpha;

void main() {
  vec3 pos = a_position;

  vec3 world_pos = (u_model * vec4(pos, 1.0)).xyz;
  vec3 to_center = world_pos - u_center;
  float dist = length(to_center.xz);
  float normalized_dist = dist / max(u_radius, 0.001);

  if (u_effect_type == 0) {

    float swirl_angle = u_time * 1.5 + normalized_dist * 3.14159;
    float swirl_strength = 0.15 * (1.0 - normalized_dist);
    pos.x += sin(swirl_angle) * swirl_strength;
    pos.z += cos(swirl_angle) * swirl_strength;

    float bob = sin(u_time * 2.0 + pos.x * 3.0) * 0.05;
    pos.y += bob + 0.1 * sin(u_time * 1.5 + pos.z * 2.0);

    float rise = max(0.0, sin(u_time * 0.5 + normalized_dist * 2.0)) * 0.3;
    pos.y += rise;

    float edge_fade = smoothstep(1.0, 0.7, normalized_dist);
    float time_pulse = 0.7 + 0.3 * sin(u_time * 1.5);
    v_alpha = edge_fade * time_pulse * u_intensity;
  } else {

    float height = a_texcoord.y;
    float angle_t = a_texcoord.x;
    float angle = angle_t * 6.28318;

    float tongue_count = 8.0;
    float tongue_id = floor(angle_t * tongue_count);
    float tongue_local = fract(angle_t * tongue_count);
    float tongue_phase = tongue_id * 1.618;

    float tongue_time = u_time + tongue_phase * 0.5;

    float sway_x = sin(tongue_time * 3.5 + height * 4.0) * 0.2 * height;
    float sway_z = cos(tongue_time * 3.0 + height * 3.5) * 0.2 * height;

    float cos_a = cos(angle);
    float sin_a = sin(angle);
    pos.x += sway_x * (-sin_a) + sway_z * cos_a;
    pos.z += sway_x * cos_a + sway_z * sin_a;

    float tongue_bulge = sin(tongue_local * 3.14159) * 0.3;
    float radial_expansion = 1.0 + tongue_bulge * (0.5 + 0.5 * height);
    pos.x *= radial_expansion;
    pos.z *= radial_expansion;

    float rise_speed = sin(tongue_time * 5.0 + height * 2.0) * 0.15;
    float vertical_scale = 1.8 + rise_speed + height * 0.5;
    pos.y *= vertical_scale;

    float turb = sin(tongue_time * 8.0 + height * 10.0) * 0.1 * height * height;
    pos.y += turb;

    float flicker = 0.85 + 0.15 * sin(tongue_time * 12.0 + height * 8.0);

    float height_fade = 1.0 - smoothstep(0.5, 1.0, height);
    float tongue_edge_fade =
        smoothstep(0.0, 0.2, tongue_local) * smoothstep(1.0, 0.8, tongue_local);
    v_alpha = height_fade * tongue_edge_fade * flicker * u_intensity * 1.5;
  }

  v_world_pos = (u_model * vec4(pos, 1.0)).xyz;
  v_normal = normalize(mat3(u_model) * a_normal);
  v_texcoord = a_texcoord;
  v_intensity = u_intensity;

  gl_Position = u_mvp * vec4(pos, 1.0);
}
