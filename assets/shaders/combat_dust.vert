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
out vec3 v_local_pos;
out float v_intensity;
out float v_alpha;

float hash12(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

float noise2(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  float a = hash12(i);
  float b = hash12(i + vec2(1.0, 0.0));
  float c = hash12(i + vec2(0.0, 1.0));
  float d = hash12(i + vec2(1.0, 1.0));
  vec2 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(vec2 p) {
  float value = 0.0;
  float amplitude = 0.5;
  for (int octave = 0; octave < 4; ++octave) {
    value += amplitude * noise2(p);
    p = p * 2.03 + vec2(13.1, 7.7);
    amplitude *= 0.5;
  }
  return value;
}

float inv_smoothstep(float edge0, float edge1, float x) {
  float lower_edge = min(edge0, edge1);
  float upper_edge = max(max(edge0, edge1), lower_edge + 0.00001);
  return 1.0 - smoothstep(lower_edge, upper_edge, x);
}

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

    float edge_fade = inv_smoothstep(0.7, 1.0, normalized_dist);
    float time_pulse = 0.7 + 0.3 * sin(u_time * 1.5);
    v_alpha = edge_fade * time_pulse * u_intensity;
  } else if (u_effect_type == 1 || u_effect_type == 4) {
    bool unit_flame = (u_effect_type == 4);
    float height = clamp(a_texcoord.y, 0.0, 1.0);
    float angle_t = a_texcoord.x;
    float angle = angle_t * 6.28318;
    float radius_factor = smoothstep(0.18, 2.8, u_radius);

    float flow_noise = fbm(vec2(angle_t * 3.6 + 4.0,
                                height * 3.8 - u_time * (1.2 + radius_factor * 0.15)));
    float curl_noise = fbm(vec2(angle_t * 7.8 - u_time * 0.65,
                                height * 6.3 - u_time * (2.2 + radius_factor * 0.25)));
    float detail_noise =
        fbm(vec2(angle_t * 14.5 + curl_noise * 0.6, height * 12.0 - u_time * 3.2));

    float lobe = 0.84 + 0.24 * sin(angle * 3.0 + u_time * 2.1 + flow_noise * 2.4) +
                 0.12 * sin(angle * 6.0 - u_time * 3.2 + detail_noise * 3.14159);
    float taper =
        mix(unit_flame ? 0.78 : 1.05, unit_flame ? 0.16 : 0.22, pow(height, 0.72));
    float smoke_expand =
        smoothstep(0.58, 1.0, height) *
        (unit_flame ? 0.02 : 0.16 + (unit_flame ? 0.04 : 0.10) * curl_noise);
    float radial_scale =
        max(0.18, taper * (0.86 + 0.24 * flow_noise) * lobe + smoke_expand);
    pos.xz *= radial_scale;

    vec2 drift_dir = vec2(cos(angle + (curl_noise - 0.5) * 1.1),
                          sin(angle + (curl_noise - 0.5) * 1.1));
    float sway =
        (sin(u_time * 3.1 + height * 4.8 + flow_noise * 2.5) * 0.11 +
         (curl_noise - 0.5) * 0.28) *
        (unit_flame ? (0.10 + height * height * 0.36)
                    : (0.18 + height * height * 0.9)) *
        (unit_flame ? (0.42 + 0.10 * radius_factor) : (0.8 + 0.25 * radius_factor));
    pos.x += drift_dir.x * sway;
    pos.z += drift_dir.y * sway;

    float lift =
        mix(unit_flame ? 0.58 : 1.2, unit_flame ? 0.92 : 2.05, radius_factor) +
        height * ((unit_flame ? 0.06 : 0.28) + (unit_flame ? 0.08 : 0.18) * flow_noise);
    pos.y *= lift;
    pos.y +=
        (detail_noise - 0.5) * (unit_flame ? 0.04 : 0.08) * (0.2 + height * height) +
        smoothstep(unit_flame ? 0.58 : 0.65, 1.0, height) *
            ((unit_flame ? 0.03 : 0.06) + (unit_flame ? 0.03 : 0.08) * curl_noise);

    float base_mask = smoothstep(0.0, 0.06, height);
    float tip_fade = 1.0 - smoothstep(unit_flame ? 0.56 : 0.7,
                                      unit_flame ? 0.86 : 1.04,
                                      height + (detail_noise - 0.5) * 0.12);
    float radius_from_axis = length(pos.xz);
    float side_fade = 1.0 - smoothstep(unit_flame ? 0.34 : 0.62,
                                       unit_flame ? 0.72 : 1.05,
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

    float height = a_texcoord.y;
    float angle_t = a_texcoord.x;
    float angle = angle_t * 6.28318;
    float t = u_time;

    float spark_count = 16.0;
    float spark_id = floor(angle_t * spark_count);
    float spark_hash = fract(sin(spark_id * 93.17 + 217.3) * 61523.7);
    float spark_hash2 = fract(sin(spark_id * 41.3 + 89.7) * 35197.1);

    float spark_speed = 3.2 + 2.8 * spark_hash;
    float spark_angle = angle + (spark_hash2 - 0.5) * 1.2;
    vec2 spark_dir = vec2(cos(spark_angle), sin(spark_angle));

    float radial_spread = t * spark_speed * (0.4 + 0.6 * spark_hash);
    pos.xz += spark_dir * radial_spread * height;

    float up_velocity = 2.5 + 3.5 * spark_hash2;
    float gravity = 12.0;
    float spark_y = height * (up_velocity * t - 0.5 * gravity * t * t);
    pos.y += max(spark_y, -0.1);

    float streak_length = 0.08 + 0.12 * spark_hash;
    pos.y += streak_length * height * (1.0 - t * 2.0);

    float life = (1.0 - smoothstep(0.15, 0.45, t));
    float spark_fade = smoothstep(0.0, 0.05, t) * life;
    float flicker = 0.7 + 0.3 * sin(t * 40.0 + spark_id * 7.0);

    v_alpha = clamp(spark_fade * flicker * u_intensity * 1.5, 0.0, 1.0);
  } else {
    vec3 normal_dir = normalize(a_normal);
    vec2 flow_uv = vec2(a_texcoord.x * 6.0 + normal_dir.y * 1.4,
                        a_texcoord.y * 6.5 - u_time * 1.8);
    float shell_noise = fbm(flow_uv);
    float detail_noise = fbm(flow_uv * 1.9 + vec2(2.7, -u_time * 0.55));
    float flare =
        sin((a_texcoord.x + a_texcoord.y) * 18.0 + u_time * 11.0 + detail_noise * 4.0);
    float pulse = 0.92 + 0.08 * sin(u_time * 9.0 + shell_noise * 3.14159);
    float shell_offset =
        (shell_noise - 0.5) * 0.18 + (detail_noise - 0.5) * 0.08 + flare * 0.03;
    pos += normal_dir * shell_offset * pulse;

    vec3 tangent = normalize(vec3(-normal_dir.z, 0.0, normal_dir.x));
    if (length(tangent) < 0.001) {
      tangent = vec3(1.0, 0.0, 0.0);
    }
    vec3 bitangent = normalize(cross(normal_dir, tangent));
    pos += tangent * (detail_noise - 0.5) * 0.05 * pulse;
    pos += bitangent * (shell_noise - 0.5) * 0.04 * pulse;

    float polar_fade = 1.0 - smoothstep(0.88, 1.0, abs(normal_dir.y));
    float spark = pow(max(detail_noise - 0.68, 0.0) * 3.4, 2.2);
    v_alpha = clamp((0.42 + 0.38 * shell_noise + spark * 0.28) *
                        (0.72 + 0.28 * polar_fade) * u_intensity,
                    0.0,
                    1.2);
  }

  v_local_pos = pos;
  v_world_pos = (u_model * vec4(pos, 1.0)).xyz;
  v_normal = normalize(mat3(u_model) * a_normal);
  v_texcoord = a_texcoord;
  v_intensity = u_intensity;

  gl_Position = u_mvp * vec4(pos, 1.0);
}
