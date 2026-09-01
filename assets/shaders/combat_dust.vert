#version 330 core
#include "noise.glsl"

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
uniform float u_span;

out vec3 v_world_pos;
out vec3 v_normal;
out vec2 v_texcoord;
out vec3 v_local_pos;
out float v_intensity;
out float v_alpha;
out float v_lick;

float inv_smoothstep(float edge0, float edge1, float x) {
  float lower_edge = min(edge0, edge1);
  float upper_edge = max(max(edge0, edge1), lower_edge + 0.00001);
  return 1.0 - smoothstep(lower_edge, upper_edge, x);
}

float soi_fbm3_fireball(vec3 p) {
  float value = 0.0;
  float amplitude = 0.5;
  for (int octave = 0; octave < 3; ++octave) {
    value += amplitude * soi_noise3(p);
    p *= 2.07;
    amplitude *= 0.5;
  }
  return value;
}

void main() {
  vec3 pos = a_position;
  v_lick = 0.0;

  vec3 world_pos = (u_model * vec4(pos, 1.0)).xyz;
  vec3 to_center = world_pos - u_center;
  float dist = length(to_center.xz);
  float normalized_dist = dist / max(u_radius, 0.001);

  if (u_effect_type == 0) {
    float height_t = clamp(a_texcoord.y, 0.0, 1.0);
    float angle_t = a_texcoord.x;
    float angle = angle_t * 6.28318;

    vec2 ring = vec2(cos(angle), sin(angle));
    float body_noise = soi_fbm_23e5ab(
        ring * 1.9 + vec2(u_time * 0.13, height_t * 2.4 - u_time * 0.21));
    float curl_noise =
        soi_fbm_23e5ab(ring * 3.7 + vec2(7.0 - u_time * 0.09, height_t * 4.6 + 3.0));

    float swirl = u_time * 0.32 + height_t * 1.35;
    vec2 dir = vec2(cos(angle + swirl), sin(angle + swirl));

    float dome = 0.26 + 0.74 * sqrt(max(0.0, 1.0 - height_t * height_t));
    float lobe = 0.88 + 0.17 * body_noise + 0.08 * curl_noise;
    pos.xz = dir * dome * lobe;

    pos.y = height_t * 0.78 * (0.86 + 0.24 * body_noise);
    pos.y += 0.03 * sin(u_time * 0.9 + angle * 2.0);

    float mound = smoothstep(0.0, 0.30, height_t) *
                  (1.0 - 0.62 * smoothstep(0.58, 1.0, height_t));
    float breathe = 0.86 + 0.14 * sin(u_time * 0.8 + body_noise * 3.1);
    v_alpha = clamp(mound * breathe * u_intensity * 0.92, 0.0, 1.05);
  } else if (u_effect_type == 1 || u_effect_type == 4) {
    bool unit_flame = (u_effect_type == 4);
    float height = clamp(a_texcoord.y, 0.0, 1.0);
    float angle_t = a_texcoord.x;
    float angle = angle_t * 6.28318;
    float radius_factor = smoothstep(0.18, 2.8, u_radius);

    float flow_noise = soi_fbm_23e5ab(vec2(
        angle_t * 3.6 + 4.0, height * 3.8 - u_time * (1.2 + radius_factor * 0.15)));
    float curl_noise =
        soi_fbm_23e5ab(vec2(angle_t * 7.8 - u_time * 0.65,
                            height * 6.3 - u_time * (2.2 + radius_factor * 0.25)));
    float detail_noise = soi_fbm_23e5ab(
        vec2(angle_t * 14.5 + curl_noise * 0.6, height * 12.0 - u_time * 3.2));

    float lobe = 0.78 + 0.27 * sin(angle * 3.0 + u_time * 2.1 + flow_noise * 2.4) +
                 0.13 * sin(angle * 6.0 - u_time * 3.2 + detail_noise * 3.14159);

    float lick_seed = soi_fbm_23e5ab(vec2(angle_t * 6.5 + 11.0, u_time * 0.85));
    float lick_fast = soi_fbm_23e5ab(vec2(angle_t * 13.0 - 4.0, u_time * 2.10 + 2.0));
    float lick = clamp(lick_seed * 0.62 + lick_fast * 0.38, 0.0, 1.0);
    float reach = mix(unit_flame ? 0.28 : 0.55, unit_flame ? 1.75 : 1.42, lick);

    float base_pinch = mix(0.30, 1.0, smoothstep(0.0, 0.20, height));
    float taper =
        mix(unit_flame ? 0.40 : 1.05, unit_flame ? 0.035 : 0.22, pow(height, 0.52)) *
        base_pinch;

    taper *= mix(unit_flame ? 0.42 : 0.68, unit_flame ? 1.34 : 1.26, lick_fast);
    float smoke_expand =
        smoothstep(0.58, 1.0, height) *
        (unit_flame ? 0.02 : 0.16 + (unit_flame ? 0.04 : 0.10) * curl_noise);
    float radial_scale = max(unit_flame ? 0.06 : 0.18,
                             taper * (0.82 + 0.28 * flow_noise) * lobe + smoke_expand);
    pos.xz *= radial_scale;

    vec2 drift_dir = vec2(cos(angle + (curl_noise - 0.5) * 1.1),
                          sin(angle + (curl_noise - 0.5) * 1.1));
    float sway =
        (sin(u_time * 3.1 + height * 4.8 + flow_noise * 2.5) * 0.11 +
         (curl_noise - 0.5) * 0.28) *
        (unit_flame ? (0.06 + height * height * 0.52)
                    : (0.18 + height * height * 0.9)) *
        (unit_flame ? (0.42 + 0.10 * radius_factor) : (0.8 + 0.25 * radius_factor));
    pos.x += drift_dir.x * sway;
    pos.z += drift_dir.y * sway;

    float lift =
        mix(unit_flame ? 1.35 : 1.2, unit_flame ? 1.95 : 2.05, radius_factor) +
        height * ((unit_flame ? 0.12 : 0.28) + (unit_flame ? 0.14 : 0.18) * flow_noise);
    pos.y *= lift * reach;
    pos.y +=
        (detail_noise - 0.5) * (unit_flame ? 0.04 : 0.08) * (0.2 + height * height) +
        smoothstep(unit_flame ? 0.58 : 0.65, 1.0, height) *
            ((unit_flame ? 0.03 : 0.06) + (unit_flame ? 0.03 : 0.08) * curl_noise);

    v_lick = lick;

    float base_mask = smoothstep(0.0, 0.17, height);
    float tip_fade = 1.0 - smoothstep(unit_flame ? 0.70 : 0.7,
                                      unit_flame ? 0.98 : 1.04,
                                      height + (detail_noise - 0.5) * 0.12);
    float radius_from_axis = length(pos.xz);
    float side_fade = 1.0 - smoothstep(unit_flame ? 0.18 : 0.62,
                                       unit_flame ? 0.48 : 1.05,
                                       radius_from_axis);
    float flicker = 0.90 + 0.10 * sin(u_time * 10.5 + angle * 4.0 + detail_noise * 4.5);
    v_alpha = clamp(base_mask * tip_fade * side_fade * flicker * u_intensity *
                        mix(1.05, 1.35, 1.0 - height),
                    0.0,
                    1.0);
  } else if (u_effect_type == 2) {

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
                    0.0,
                    1.0);
  } else if (u_effect_type == 5) {
    float t = u_time;
    float along = clamp(a_texcoord.x, 0.0, 1.0);
    float expansion = mix(0.40, 1.0, smoothstep(0.0, 0.09, t));
    pos *= expansion;
    pos.y += t * (0.08 + 0.15 * along) - 0.5 * 1.4 * t * t * along;

    float life = 1.0 - smoothstep(0.08, 0.28, t);
    float spark_fade = smoothstep(0.0, 0.05, t) * life;
    float flicker = 0.82 + 0.18 * sin(t * 45.0 + along * 11.0);
    v_alpha = clamp(spark_fade * flicker * u_intensity, 0.0, 1.0);
  } else if (u_effect_type == 6) {
    float t = clamp(u_time, 0.0, 1.0);
    float span = clamp(abs(u_span), 0.02, 1.0);
    bool ring = abs(u_span) >= 0.999;
    float centered = (a_texcoord.x - 0.5) * 2.0;
    float inside = step(abs(centered), span);
    float along = clamp(centered / span * 0.5 + 0.5, 0.0, 1.0);
    if (u_span < 0.0) {
      along = 1.0 - along;
    }
    float across = a_texcoord.y;

    float head = ring ? 1.0 : smoothstep(0.0, 0.42, t);
    float tail = ring ? 0.0 : smoothstep(0.30, 1.0, t) * 0.85;
    float head_mask = 1.0 - smoothstep(head - 0.07, head + 0.01, along);
    float head_soft = 1.0 - smoothstep(head - 0.16, head, along);
    float tail_soft = smoothstep(tail, tail + 0.22, along);
    float wipe = ring ? 1.0 : head_mask * (0.35 + 0.65 * head_soft) * tail_soft;

    float grow = ring ? mix(0.40, 1.0, smoothstep(0.0, 0.6, t)) : 1.0;
    pos *= grow;
    pos.y += ring ? 0.0 : (across - 0.5) * 0.04;

    float life =
        ring ? 1.0 - smoothstep(0.25, 0.80, t) : 1.0 - smoothstep(0.55, 1.0, t);
    float edge =
        ring ? (1.0 - smoothstep(0.55, 1.0, across)) * smoothstep(0.0, 0.25, across)
             : smoothstep(0.0, 0.35, across);
    v_alpha = clamp(inside * wipe * life * edge * u_intensity, 0.0, 1.0);
    if (inside < 0.5) {
      pos = vec3(0.0);
    }
  } else {
    vec3 normal_dir = normalize(a_normal);

    vec3 roll = normal_dir * 2.4 - vec3(0.0, u_time * 1.05, 0.0);
    float body_noise = soi_fbm3_fireball(roll);
    float detail_noise =
        soi_fbm3_fireball(normal_dir * 5.6 + vec3(u_time * 0.6, -u_time * 1.9, 3.0));

    float lobe_noise =
        soi_fbm3_fireball(normal_dir * 1.15 + vec3(0.0, -u_time * 0.42, 7.0));
    float pulse = 0.94 + 0.06 * sin(u_time * 8.0 + body_noise * 3.14159);

    float shell_offset = (lobe_noise - 0.5) * 0.46 + (body_noise - 0.5) * 0.26 +
                         (detail_noise - 0.5) * 0.13 - 0.04;
    pos += normal_dir * shell_offset * pulse;

    vec3 tangent = normalize(vec3(-normal_dir.z, 0.0, normal_dir.x));
    if (length(tangent) < 0.001) {
      tangent = vec3(1.0, 0.0, 0.0);
    }
    vec3 bitangent = normalize(cross(normal_dir, tangent));
    pos += tangent * (detail_noise - 0.5) * 0.07 * pulse;
    pos += bitangent * (body_noise - 0.5) * 0.06 * pulse;

    float rise = smoothstep(-0.85, 1.0, normal_dir.y);
    float crown = smoothstep(0.05, 1.0, normal_dir.y);
    float belly = 1.0 - smoothstep(-1.0, 0.15, normal_dir.y);

    pos.y += (0.16 + 0.13 * body_noise) * rise + 0.14 * crown;
    pos.y -= 0.10 * belly;
    pos.xz *= mix(0.72, 1.18, rise) * (1.0 + 0.10 * lobe_noise);

    pos.xz += normalize(pos.xz + vec2(1.0e-4)) * crown * (0.06 + 0.10 * lobe_noise);

    v_alpha = clamp((0.55 + 0.45 * body_noise) * u_intensity, 0.0, 1.3);
  }

  v_local_pos = pos;
  v_world_pos = (u_model * vec4(pos, 1.0)).xyz;
  v_normal = normalize(mat3(u_model) * a_normal);
  v_texcoord = a_texcoord;
  v_intensity = u_intensity;

  gl_Position = u_mvp * vec4(pos, 1.0);
}
