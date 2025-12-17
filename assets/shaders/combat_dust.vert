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
  } else if (u_effect_type == 1) {

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
  } else {

    float height = a_texcoord.y;
    float angle_t = a_texcoord.x;
    float angle = angle_t * 6.28318;
    float t = u_time;

    float phase = smoothstep(0.0, 0.15, t);
    float decay = 1.0 - smoothstep(2.5, 5.0, t);
    float life = phase * decay;

    float chunk_id = floor(angle_t * 24.0);
    float chunk_hash = fract(sin(chunk_id * 127.1 + 311.7) * 43758.5453);
    float chunk_speed = 0.7 + chunk_hash * 0.6;
    float chunk_angle_offset = (chunk_hash - 0.5) * 0.4;

    float ejection_angle = angle + chunk_angle_offset;
    vec2 dir = vec2(cos(ejection_angle), sin(ejection_angle));

    float base_spread = mix(0.3, 2.2, height);
    float time_spread = t * chunk_speed * 1.8;
    float spread = base_spread + time_spread * (0.6 + 0.4 * chunk_hash);

    float turbulence = sin(t * 3.5 + chunk_id * 2.1) * 0.15 * (1.0 - height);
    vec2 perp = vec2(-dir.y, dir.x);
    pos.xz += dir * spread + perp * turbulence;

    float initial_velocity = 4.5 + 2.0 * chunk_hash;
    float gravity_accel = 9.8;
    float upward = height * initial_velocity * t - 0.5 * gravity_accel * t * t;
    upward = max(upward, -0.3);

    float dust_rise = (1.0 - height) * 0.8 * t * decay;
    pos.y += upward + dust_rise;

    float rotation = t * (2.0 + chunk_hash * 3.0);
    float wobble = sin(rotation) * 0.1 * height;
    pos.x += wobble * dir.y;
    pos.z -= wobble * dir.x;

    float radial = length(pos.xz);
    float radial_fade = 1.0 - smoothstep(1.5, 3.0, radial);

    float height_fade =
        smoothstep(0.0, 0.1, height) * (1.0 - smoothstep(0.7, 1.0, height));

    float dust_density = (1.0 - height) * 0.6 + 0.4;

    float flicker = 0.85 + 0.15 * sin(t * 8.0 + chunk_id * 4.0);

    v_alpha = clamp(life * radial_fade * height_fade * dust_density * flicker *
                        u_intensity * 1.2,
                    0.0, 1.0);
  }

  v_world_pos = (u_model * vec4(pos, 1.0)).xyz;
  v_normal = normalize(mat3(u_model) * a_normal);
  v_texcoord = a_texcoord;
  v_intensity = u_intensity;

  gl_Position = u_mvp * vec4(pos, 1.0);
}
