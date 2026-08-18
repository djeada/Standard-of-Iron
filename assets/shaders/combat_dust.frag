#version 330 core
#include "noise.glsl"

in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_local_pos;
in float v_intensity;
in float v_alpha;
in float v_lick;

out vec4 frag_color;

uniform vec3 u_dust_color;
uniform float u_time;
uniform vec3 u_center;
uniform float u_radius;
uniform int u_effect_type;
uniform vec3 u_camera_pos;

float inv_smoothstep(float edge0, float edge1, float x) {
  float lower_edge = min(edge0, edge1);
  float upper_edge = max(max(edge0, edge1), lower_edge + 0.00001);
  return 1.0 - smoothstep(lower_edge, upper_edge, x);
}

float soi_fireball_turbulence(vec3 p) {
  float value = 0.0;
  float amplitude = 0.5;
  for (int octave = 0; octave < 4; ++octave) {
    value += amplitude * soi_noise3(p);
    p *= 2.03;
    amplitude *= 0.5;
  }
  return value;
}

vec3 soi_fire_ramp(float temperature) {
  float t = clamp(temperature, 0.0, 1.0);
  vec3 soot = vec3(0.045, 0.030, 0.026);
  vec3 ember = vec3(0.62, 0.075, 0.012);
  vec3 orange = vec3(1.45, 0.42, 0.045);
  vec3 amber = vec3(2.30, 1.15, 0.18);
  vec3 white_hot = vec3(3.10, 2.45, 1.65);

  vec3 color = mix(soot, ember, smoothstep(0.02, 0.28, t));
  color = mix(color, orange, smoothstep(0.24, 0.56, t));
  color = mix(color, amber, smoothstep(0.54, 0.80, t));
  color = mix(color, white_hot, smoothstep(0.80, 1.0, t));
  return color;
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

    float body_noise = soi_fbm_23e5ab(
        vec2(v_texcoord.x * 4.6 + 3.0,
             flame_height * 4.4 - u_time * (1.5 + radius_factor * 0.18)));
    float curl_noise = soi_fbm_23e5ab(
        vec2(v_texcoord.x * 9.8 - u_time * 0.75, flame_height * 7.6 - u_time * 2.6));
    float ember_noise = soi_fbm_23e5ab(
        vec2(v_texcoord.x * 18.0 + u_time * 0.55, flame_height * 18.0 - u_time * 5.2));
    float soot_noise = soi_fbm_23e5ab(
        vec2(v_texcoord.x * 11.0 - u_time * 0.25, flame_height * 13.0 + 6.0));

    vec3 white_hot = vec3(2.05, 1.62, 0.92);
    vec3 hot_core = vec3(1.78, 0.74, 0.11);

    vec3 orange_body = vec3(1.18, 0.38, 0.05);
    vec3 ember_red = vec3(0.62, 0.09, 0.02);
    vec3 smoke_tip = vec3(0.10, 0.085, 0.075);

    float heat = pow(max(0.0, 1.0 - flame_height), 1.7);
    float core_mask = inv_smoothstep(0.12, 0.82, axis_radius);
    color = mix(orange_body, ember_red, smoothstep(0.38, 1.0, flame_height));
    color = mix(color, hot_core, heat * (0.55 + 0.25 * body_noise));
    color = mix(color,
                white_hot,
                core_mask * pow(max(0.0, 1.0 - flame_height), 2.6) *
                    (unit_flame ? 0.45 : 1.0));

    color = mix(color,
                smoke_tip,
                smoothstep(unit_flame ? 0.76 : 0.58, 1.0, flame_height) *
                    ((unit_flame ? 0.10 : 0.58) + 0.34 * soot_noise));

    float ember_band = smoothstep(0.05, 0.24, flame_height) *
                       (1.0 - smoothstep(0.22, 0.65, flame_height));
    float ember_sparks = pow(max(ember_noise - 0.73, 0.0) * 4.2, 2.8) * ember_band;
    color += vec3(2.0, 1.1, 0.28) * ember_sparks;
    color *=
        (0.82 + 0.22 * body_noise + 0.14 * curl_noise) * v_intensity *
        (unit_flame ? (0.96 + 0.06 * radius_factor) : (1.02 + 0.10 * radius_factor));

    float edge_soften =
        inv_smoothstep(unit_flame ? 0.03 : 0.15, unit_flame ? 0.42 : 1.15, axis_radius);
    float height_fade =
        1.0 -
        smoothstep(unit_flame ? 0.58 : 0.72, unit_flame ? 0.97 : 1.04, flame_height);

    float tongues = smoothstep(0.18, 0.72, body_noise * 0.55 + curl_noise * 0.45);

    float solidity = 1.0 - smoothstep(0.04, unit_flame ? 0.40 : 0.62, flame_height);
    float cut = mix(tongues * tongues * (unit_flame ? tongues : 1.0),
                    1.0,
                    solidity * (unit_flame ? 0.80 : 0.92));

    float lick_fade =
        1.0 -
        smoothstep(mix(0.34, 0.86, v_lick), mix(0.62, 1.05, v_lick), flame_height);

    float flame_alpha = v_alpha * edge_soften * height_fade * cut * lick_fade *
                        (0.86 + 0.14 * curl_noise) * (unit_flame ? 0.66 : 1.0);

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

    vec3 debris_tint = max(u_dust_color, vec3(0.03));
    float debris_luma = dot(debris_tint, vec3(0.30, 0.59, 0.11));
    color *=
        mix(vec3(1.0), clamp(debris_tint / max(debris_luma, 0.05), 0.35, 1.9), 0.75);

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
    vec3 view_dir = normalize(u_camera_pos - v_world_pos);

    float facing = clamp(dot(normalize(v_normal), view_dir), 0.0, 1.0);
    float core = pow(facing, 1.55);

    float detail_scale = mix(1.0, 2.3, smoothstep(0.12, 1.10, u_radius));
    vec3 roll = shell_dir * 2.7 * detail_scale - vec3(0.0, u_time * 1.15, 0.0);
    float body = soi_fireball_turbulence(roll);
    float detail = soi_fireball_turbulence(shell_dir * 6.9 * detail_scale +
                                           vec3(u_time * 0.7, -u_time * 2.1, 0.0));
    float soot_noise = soi_fireball_turbulence(shell_dir * 1.9 * detail_scale +
                                               vec3(-u_time * 0.3, u_time * 0.2, 4.0));

    float caller_heat =
        dot(u_dust_color, vec3(0.30, 0.55, 0.15)) + 0.35 * u_dust_color.b;
    float feed = 1.0 - smoothstep(-0.75, 0.55, shell_dir.y);
    float temperature = clamp(core * (0.52 + 0.48 * body) + 0.24 * detail - 0.16 +
                                  (caller_heat - 0.52) * 0.58 + 0.14 * feed,
                              0.0,
                              1.0);
    color = soi_fire_ramp(temperature);

    color *= mix(1.0, 0.74 + 0.48 * detail, smoothstep(0.32, 0.88, temperature));

    float crown = smoothstep(-0.10, 0.95, shell_dir.y);
    float soot_mask = (1.0 - core * 0.75) * smoothstep(0.22, 0.78, soot_noise) *
                      mix(0.35, 1.35, crown) *
                      (1.0 - 0.55 * smoothstep(0.55, 0.95, temperature));
    soot_mask = clamp(soot_mask, 0.0, 1.0);
    color = mix(color, vec3(0.045, 0.035, 0.032), soot_mask * 0.92);

    float sparks = pow(max(detail - 0.68, 0.0) * 3.4, 2.4);
    color += vec3(2.9, 1.35, 0.34) * sparks * (0.30 + 0.70 * core);

    color *= mix(vec3(1.0),
                 clamp(u_dust_color * 1.5 + vec3(0.30, 0.24, 0.18), 0.45, 1.55),
                 0.32);
    color *= v_intensity * (0.94 + 0.06 * sin(u_time * 17.0 + body * 6.0));

    float thickness = pow(facing, 1.9);
    float dissolve = smoothstep(0.16, 0.62, body * 0.62 + detail * 0.38);
    float fireball_alpha =
        clamp(v_alpha * thickness * mix(0.22, 1.0, dissolve) *
                  (0.24 + 0.76 * smoothstep(0.04, 0.72, temperature) + sparks * 0.30) *
                  (1.0 - soot_mask * 0.30),
              0.0,
              0.86);
    color = clamp(color, 0.0, 4.0) * fireball_alpha;
    frag_color = vec4(color, fireball_alpha);
  } else if (u_effect_type == 5) {
    float along = clamp(v_texcoord.x, 0.0, 1.0);
    float across = abs(v_texcoord.y * 2.0 - 1.0);
    float spark_age = clamp(u_time * 3.0, 0.0, 1.0);

    vec3 accent = max(u_dust_color, vec3(0.02));

    vec3 white_hot = mix(accent * 2.6, vec3(3.0, 2.8, 2.4), 0.42);
    vec3 hot_accent = accent * 2.4;
    vec3 cool_accent = accent * 0.85;

    float core_heat = pow(max(0.0, 1.0 - along), 2.0);
    color = mix(hot_accent, white_hot, core_heat * (1.0 - spark_age * 0.6));
    color = mix(color, cool_accent, along * spark_age);

    float glint = pow(max(0.0,
                          sin(v_texcoord.x * 40.0 + u_time * 25.0) *
                              sin(v_texcoord.y * 30.0 - u_time * 18.0)),
                      8.0);
    color += mix(vec3(2.0, 1.8, 1.2), accent * 3.2, 0.5) * glint * (1.0 - spark_age);

    color *= v_intensity * 1.4;
    color = clamp(color, 0.0, 4.0);

    float edge_fade = 1.0 - smoothstep(0.55, 1.0, across);
    float tip_fade = 1.0 - smoothstep(0.72, 1.0, along);
    float spark_alpha = v_alpha * edge_fade * (0.72 + 0.28 * tip_fade);
    frag_color = vec4(color, clamp(spark_alpha, 0.0, 1.0));
  } else {

    color = u_dust_color;
    frag_color = vec4(color, final_alpha * 0.6);
  }
}
