#version 330 core
#include "environment_lighting.glsl"
#include "noise.glsl"
#include "tonemap.glsl"

in vec2 v_uv;

uniform sampler2D u_scene;
uniform sampler2D u_bloom;
uniform sampler2D u_depth;
uniform float u_bloom_intensity;
uniform float u_vignette_strength;
uniform vec2 u_depth_range;
uniform vec2 u_inverse_resolution;
uniform float u_ground_ao_radius;
uniform float u_ground_ao_strength;
uniform mat4 u_inverse_view_proj;
uniform vec3 u_camera_position;
uniform vec2 u_fog_range;
uniform float u_time;
uniform int u_mist_count;
uniform vec4 u_mist_seg[24];
uniform vec4 u_mist_info[24];

out vec4 frag_color;

const float k_vignette_inner = 0.55;
const float k_vignette_outer = 1.35;
const vec3 k_vignette_tint = vec3(0.86, 0.89, 0.97);
const int k_ao_tap_count = 8;
const int k_ao_ring_count = 3;
const float k_ao_ring_spread[3] = float[3](2.5, 8.0, 18.0);
const float k_ao_ring_weight[3] = float[3](1.0, 0.78, 0.46);
const float k_ao_ring_reach = 1.7;
const float k_ao_far_cutoff = 26.0;
const vec3 k_ao_tint = vec3(0.72, 0.76, 0.86);

const float k_fog_horizon_weight = 0.72;
const float k_fog_horizon_gain = 0.60;
const float k_fog_desaturation = 0.35;

const float k_mist_water_ceiling = 1.7;
const float k_mist_miasma_ceiling = 2.8;
const float k_mist_bank_reach = 5.0;
const vec3 k_mist_water_lift = vec3(1.18, 1.18, 1.16);
const vec3 k_mist_miasma_tint = vec3(0.95, 0.72, 1.45);
const vec3 k_mist_miasma_floor = vec3(0.012, 0.008, 0.028);

const vec2 k_ao_taps[8] = vec2[8](vec2(1.0, 0.0),
                                  vec2(0.7071, 0.7071),
                                  vec2(0.0, 1.0),
                                  vec2(-0.7071, 0.7071),
                                  vec2(-1.0, 0.0),
                                  vec2(-0.7071, -0.7071),
                                  vec2(0.0, -1.0),
                                  vec2(0.7071, -0.7071));

float linear_depth(vec2 uv) {
  float raw = texture(u_depth, uv).r;
  float near_plane = u_depth_range.x;
  float far_plane = u_depth_range.y;
  float ndc = raw * 2.0 - 1.0;
  return (2.0 * near_plane * far_plane) /
         (far_plane + near_plane - ndc * (far_plane - near_plane));
}

float grounding_occlusion() {
  float center = linear_depth(v_uv);
  if (center >= u_depth_range.y * 0.999) {
    return 0.0;
  }

  float occlusion = 0.0;
  float weight_total = 0.0;
  for (int ring = 0; ring < k_ao_ring_count; ++ring) {
    float spread = k_ao_ring_spread[ring];
    float weight = k_ao_ring_weight[ring];
    float reach = u_ground_ao_radius * (1.0 + float(ring) * k_ao_ring_reach);
    for (int tap = 0; tap < k_ao_tap_count; ++tap) {
      vec2 offset = k_ao_taps[tap] * u_inverse_resolution * spread;
      float neighbour = linear_depth(v_uv + offset);
      float difference = center - neighbour;
      float contribution =
          smoothstep(0.0, reach, difference) *
          (1.0 - smoothstep(reach, reach * k_ao_far_cutoff, difference));
      occlusion += contribution * weight;
    }
    weight_total += weight * float(k_ao_tap_count);
  }
  return clamp(occlusion / max(weight_total, 1e-4), 0.0, 1.0);
}

vec3 world_position(vec2 uv) {
  float raw = texture(u_depth, uv).r;
  vec4 ndc = vec4(uv * 2.0 - 1.0, raw * 2.0 - 1.0, 1.0);
  vec4 world = u_inverse_view_proj * ndc;
  return world.xyz / world.w;
}

float scene_fog_amount(vec3 world) {
  vec3 to_camera = u_camera_position - world;
  float view_distance = max(length(to_camera), 1e-4);
  float horizon = smoothstep(0.20, 0.88, 1.0 - abs(to_camera.y) / view_distance);
  return atmospheric_fog_amount(view_distance,
                                u_fog_range.x,
                                u_fog_range.y,
                                k_fog_horizon_weight,
                                k_fog_horizon_gain * horizon);
}

vec2 ground_mist(vec3 world) {
  if (u_mist_count == 0) {
    return vec2(0.0);
  }
  float drift =
      soi_value_noise_e2c097(world.xz * 0.09 + vec2(u_time * 0.045, -u_time * 0.028));
  float swirl =
      soi_value_noise_e2c097(world.xz * 0.23 - vec2(u_time * 0.020, u_time * 0.034));

  float water = 0.0;
  float miasma = 0.0;
  for (int i = 0; i < u_mist_count; ++i) {
    vec2 a = u_mist_seg[i].xy;
    vec2 b = u_mist_seg[i].zw;
    vec2 pa = world.xz - a;
    vec2 ba = b - a;
    float t = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-4), 0.0, 1.0);
    float edge = length(pa - ba * t) - u_mist_info[i].x;
    float ragged_edge = edge + (drift - 0.5) * 2.6;
    float lateral = 1.0 - smoothstep(0.0, k_mist_bank_reach, ragged_edge);
    lateral *= lateral;
    bool is_miasma = u_mist_info[i].z > 0.5;
    float ceiling = is_miasma ? k_mist_miasma_ceiling : k_mist_water_ceiling;
    float rise = world.y - u_mist_info[i].w;
    float vertical = 1.0 - smoothstep(0.15 * ceiling, ceiling, rise);
    float amount = u_mist_info[i].y * lateral * vertical;
    if (is_miasma) {
      miasma = max(miasma, amount);
    } else {
      water = max(water, amount);
    }
  }
  float patch = smoothstep(0.30, 0.72, 0.62 * drift + 0.38 * swirl);
  float breakup = 0.25 + 0.75 * patch;
  return clamp(vec2(water, miasma) * breakup, 0.0, 1.0);
}

void main() {
  vec3 scene = texture(u_scene, v_uv).rgb;
  vec3 bloom = texture(u_bloom, v_uv).rgb;
  vec3 combined = scene + bloom * u_bloom_intensity;

  float occlusion = clamp(grounding_occlusion() * u_ground_ao_strength, 0.0, 1.0);
  combined = mix(combined, combined * k_ao_tint, occlusion);

  if (linear_depth(v_uv) < u_depth_range.y * 0.999) {
    vec3 world = world_position(v_uv);

    vec2 mist = ground_mist(world);
    combined = mix(combined, environment_fog_color() * k_mist_water_lift, mist.x);
    combined = mix(combined,
                   environment_fog_color() * k_mist_miasma_tint + k_mist_miasma_floor,
                   mist.y);

    float fog = scene_fog_amount(world);
    float haze_luma = dot(combined, vec3(0.2126, 0.7152, 0.0722));
    combined = mix(combined, vec3(haze_luma), fog * k_fog_desaturation);
    combined = mix(combined, environment_fog_color(), fog);
  }

  vec3 graded = soi_finalize(combined);

  vec2 centered = v_uv * 2.0 - 1.0;
  float falloff = smoothstep(k_vignette_inner, k_vignette_outer, length(centered));
  float vignette = 1.0 - falloff * u_vignette_strength;
  graded = mix(graded * k_vignette_tint, graded, vignette);
  graded *= vignette;

  frag_color = vec4(clamp(graded, 0.0, 1.0), 1.0);
}
