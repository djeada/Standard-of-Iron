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
uniform int u_effect_type; // 0 = Dust, 1 = Flame

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
    // Dust effect - swirl and rise
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
    // Flame effect - vertical movement and flicker
    float flame_wave = sin(u_time * 3.0 + pos.x * 5.0) * 0.08;
    pos.x += flame_wave;
    pos.z += cos(u_time * 3.5 + pos.z * 5.0) * 0.08;

    // Flames rise upward
    float flame_rise = a_texcoord.y * 1.5;
    pos.y += flame_rise;

    // Flicker effect
    float flicker = 0.9 + 0.1 * sin(u_time * 8.0 + normalized_dist * 10.0);
    pos.y *= flicker;

    // Fade at edges and top
    float edge_fade = smoothstep(1.0, 0.5, normalized_dist);
    float height_fade = smoothstep(1.0, 0.3, a_texcoord.y);
    float flicker_alpha = 0.8 + 0.2 * sin(u_time * 5.0);
    v_alpha = edge_fade * height_fade * flicker_alpha * u_intensity;
  }

  v_world_pos = (u_model * vec4(pos, 1.0)).xyz;
  v_normal = normalize(mat3(u_model) * a_normal);
  v_texcoord = a_texcoord;
  v_intensity = u_intensity;

  gl_Position = u_mvp * vec4(pos, 1.0);
}
