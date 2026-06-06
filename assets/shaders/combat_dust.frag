#version 330 core

in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_local_pos;
in float v_intensity;
in float v_alpha;

out vec4 frag_color;

uniform vec3 u_dust_color;
uniform float u_time;
uniform vec3 u_center;
uniform float u_radius;
uniform int u_effect_type;

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
  return 1.0 - smoothstep(edge0, edge1, x);
}

void main() {

  float noise1 = fract(sin(dot(v_texcoord * 10.0, vec2(12.9898, 78.233))) * 43758.5453);
  float noise2 = fract(
      sin(dot(v_texcoord * 15.0 + u_time * 0.1, vec2(93.9898, 67.345))) * 23421.631);
  float combined_noise = (noise1 + noise2) * 0.5;

  vec2 centered_uv = v_texcoord * 2.0 - 1.0;
  float dist_from_center = length(centered_uv);
  float particle_alpha = inv_smoothstep(0.3, 1.0, dist_from_center);

  float final_alpha = v_alpha * particle_alpha * (0.5 + 0.5 * combined_noise);

  vec3 color = u_dust_color;

  if (u_effect_type == 0) {

    color = u_dust_color;
    color = mix(color, color * 0.8, noise1 * 0.3);

    float scatter = max(0.0, v_normal.y) * 0.2;
    color += vec3(scatter);

    frag_color = vec4(color, final_alpha * 0.6);
  } else if (u_effect_type == 1 || u_effect_type == 4) {
    bool unit_flame = (u_effect_type == 4);
    float flame_height = clamp(v_texcoord.y, 0.0, 1.0);
    float axis_radius = length(v_local_pos.xz);
    float radius_factor = smoothstep(0.18, 2.8, u_radius);

    float body_noise =
        fbm(vec2(v_texcoord.x * 4.6 + 3.0,
                 flame_height * 4.4 - u_time * (1.5 + radius_factor * 0.18)));
    float curl_noise = fbm(
        vec2(v_texcoord.x * 9.8 - u_time * 0.75, flame_height * 7.6 - u_time * 2.6));
    float ember_noise = fbm(
        vec2(v_texcoord.x * 18.0 + u_time * 0.55, flame_height * 18.0 - u_time * 5.2));
    float soot_noise =
        fbm(vec2(v_texcoord.x * 11.0 - u_time * 0.25, flame_height * 13.0 + 6.0));

    vec3 white_hot = vec3(2.10, 1.85, 1.35);
    vec3 hot_core = vec3(1.75, 1.05, 0.28);
    vec3 orange_body = vec3(1.28, 0.48, 0.08);
    vec3 ember_red = vec3(0.72, 0.12, 0.03);
    vec3 smoke_tip = vec3(0.14, 0.14, 0.16);

    float heat = pow(max(0.0, 1.0 - flame_height), 1.7);
    float core_mask = inv_smoothstep(0.12, 0.82, axis_radius);
    color = mix(orange_body, ember_red, smoothstep(0.38, 1.0, flame_height));
    color = mix(color, hot_core, heat * (0.55 + 0.25 * body_noise));
    color = mix(color, white_hot, core_mask * pow(max(0.0, 1.0 - flame_height), 2.6));
    color = mix(color,
                smoke_tip,
                smoothstep(unit_flame ? 0.72 : 0.72, 1.0, flame_height) *
                    ((unit_flame ? 0.10 : 0.45) + 0.35 * soot_noise));

    float ember_band = smoothstep(0.05, 0.24, flame_height) *
                       (1.0 - smoothstep(0.22, 0.65, flame_height));
    float ember_sparks = pow(max(ember_noise - 0.73, 0.0) * 4.2, 2.8) * ember_band;
    color += vec3(2.0, 1.1, 0.28) * ember_sparks;
    color += vec3(0.10, 0.20, 0.45) * core_mask *
             pow(max(0.0, 1.0 - flame_height), 3.0) * 0.22;

    color *=
        (0.82 + 0.22 * body_noise + 0.14 * curl_noise) * v_intensity *
        (unit_flame ? (1.18 + 0.08 * radius_factor) : (1.02 + 0.10 * radius_factor));

    float edge_soften =
        inv_smoothstep(unit_flame ? 0.10 : 0.15, unit_flame ? 0.74 : 1.15, axis_radius);
    float height_fade =
        1.0 -
        smoothstep(unit_flame ? 0.60 : 0.76, unit_flame ? 0.90 : 1.04, flame_height);
    float flame_alpha = v_alpha * edge_soften * height_fade * (0.9 + 0.1 * curl_noise) *
                        (unit_flame ? 0.82 : 1.0);

    color = clamp(color, 0.0, 2.8);
    frag_color = vec4(color, clamp(flame_alpha, 0.0, 1.0));
  } else if (u_effect_type == 2) {

    float height = v_texcoord.y;
    float angle_t = v_texcoord.x;
    float t = u_time * 0.5;

    float chunk_count = 24.0;
    float chunk_id = floor(angle_t * chunk_count);
    float chunk_local = fract(angle_t * chunk_count);
    float chunk_hash = fract(sin(chunk_id * 127.1 + 311.7) * 43758.5453);

    float noise1 = fract(sin(dot(v_texcoord * 20.0 + t * 0.3, vec2(12.9898, 78.233))) *
                         43758.5453);
    float noise2 =
        fract(sin(dot(v_texcoord * 45.0 + chunk_id, vec2(93.989, 67.345))) * 23421.6);
    float combined_noise = mix(noise1, noise2, 0.5);

    vec3 dust_dark = vec3(0.35, 0.30, 0.22);
    vec3 dust_mid = vec3(0.55, 0.48, 0.38);
    vec3 dust_light = vec3(0.75, 0.68, 0.55);
    vec3 rock_dark = vec3(0.25, 0.22, 0.18);
    vec3 rock_mid = vec3(0.40, 0.36, 0.30);

    float is_rock = step(0.6, chunk_hash) * step(height, 0.5);

    if (is_rock > 0.5) {
      color = mix(rock_dark, rock_mid, combined_noise);
      color *= 0.85 + 0.15 * sin(t * 5.0 + chunk_id);
    } else {
      if (height < 0.25) {
        color = mix(dust_dark, dust_mid, height / 0.25);
      } else if (height < 0.6) {
        color = mix(dust_mid, dust_light, (height - 0.25) / 0.35);
      } else {
        color = mix(dust_light, vec3(0.9, 0.85, 0.75), (height - 0.6) / 0.4);
      }

      float billow = 0.9 + 0.1 * sin(t * 2.0 + chunk_id * 1.5 + height * 4.0);
      color *= billow;
    }

    color *= 0.9 + 0.2 * combined_noise;

    float phase = smoothstep(0.0, 0.15, t);
    float decay = 1.0 - smoothstep(2.5, 5.0, t);

    float core_glow = (1.0 - height) * (1.0 - smoothstep(0.0, 0.4, t)) * 0.5;
    color += vec3(1.0, 0.8, 0.4) * core_glow;

    color *= v_intensity * 1.1;

    float chunk_fade =
        smoothstep(0.0, 0.15, chunk_local) * inv_smoothstep(0.85, 1.0, chunk_local);
    float density = 0.5 + 0.5 * (1.0 - height);

    float radial = length(v_world_pos.xz - u_center.xz) / max(u_radius, 0.01);
    float radial_fade = 1.0 - smoothstep(0.8, 2.0, radial);

    float impact_alpha =
        v_alpha * chunk_fade * density * radial_fade * (0.8 + 0.2 * combined_noise);

    impact_alpha = clamp(impact_alpha * 1.3, 0.0, 0.95);

    color = clamp(color, 0.0, 1.5);
    frag_color = vec4(color, impact_alpha);
  } else if (u_effect_type == 3) {

    vec3 shell_dir = normalize(v_local_pos);
    float shell_noise =
        fbm(vec2(v_texcoord.x * 7.5 - u_time * 1.9, v_texcoord.y * 6.4 - u_time * 2.6));
    float detail_noise = fbm(
        vec2(v_texcoord.x * 15.0 + u_time * 0.8, v_texcoord.y * 12.0 - u_time * 4.4));
    float hot_streak = pow(max(detail_noise - 0.70, 0.0) * 3.5, 2.6);
    float polar = 1.0 - smoothstep(0.65, 1.0, abs(shell_dir.y));
    float shell_mask = smoothstep(0.15, 0.95, shell_noise);

    vec3 ember_shell = vec3(0.95, 0.16, 0.04);
    vec3 orange_shell = vec3(1.25, 0.55, 0.10);
    vec3 hot_shell = vec3(2.10, 1.25, 0.32);
    color = mix(ember_shell, orange_shell, shell_noise);
    color = mix(color, hot_shell, shell_mask * 0.55 + hot_streak * 0.35);
    color += vec3(0.10, 0.26, 0.60) * pow(max(0.0, shell_noise - 0.45), 2.0) * 0.25;
    color += vec3(2.60, 1.35, 0.38) * hot_streak * (0.65 + 0.35 * polar);

    float glow = 0.5 + 0.5 * sin(u_time * 12.0 + (v_texcoord.x + v_texcoord.y) * 18.0 +
                                 shell_noise * 4.0);
    color *= (0.85 + 0.20 * glow) * v_intensity;

    float fireball_alpha =
        clamp(v_alpha * (0.55 + 0.45 * shell_mask + hot_streak * 0.40), 0.0, 1.0);
    color = clamp(color, 0.0, 3.4) * fireball_alpha;
    frag_color = vec4(color, fireball_alpha);
  } else if (u_effect_type == 5) {

    float height = clamp(v_texcoord.y, 0.0, 1.0);
    float spark_age = clamp(u_time * 3.0, 0.0, 1.0);

    vec3 white_hot = vec3(3.0, 2.8, 2.2);
    vec3 hot_orange = vec3(2.4, 1.2, 0.15);
    vec3 cool_red = vec3(1.4, 0.3, 0.05);

    float core_heat = pow(max(0.0, 1.0 - height), 2.0);
    color = mix(hot_orange, white_hot, core_heat * (1.0 - spark_age * 0.6));
    color = mix(color, cool_red, height * spark_age);

    float glint = pow(max(0.0,
                          sin(v_texcoord.x * 40.0 + u_time * 25.0) *
                              sin(v_texcoord.y * 30.0 - u_time * 18.0)),
                      8.0);
    color += vec3(2.0, 1.8, 1.2) * glint * (1.0 - spark_age);

    color *= v_intensity * 1.4;
    color = clamp(color, 0.0, 4.0);

    float spark_alpha = v_alpha * (0.8 + 0.2 * (1.0 - height));
    frag_color = vec4(color, clamp(spark_alpha, 0.0, 1.0));
  } else {

    color = u_dust_color;
    frag_color = vec4(color, final_alpha * 0.6);
  }
}
