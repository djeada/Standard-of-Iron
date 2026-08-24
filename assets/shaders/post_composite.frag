#version 330 core
#include "environment_lighting.glsl"
#include "noise.glsl"
#include "tonemap.glsl"

in vec2 v_uv;

uniform sampler2D u_scene;
uniform sampler2D u_bloom;
uniform sampler2D u_depth;
uniform sampler2D u_rays;
uniform float u_godray_strength;
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
uniform vec4 u_ground_fog;

out vec4 frag_color;

const float k_vignette_inner = 0.55;
const float k_vignette_outer = 1.35;
const vec3 k_vignette_tint = vec3(0.92, 0.84, 0.72);
const float k_grain_strength = 0.028;
const float k_night_exposure_lift = 1.55;
const float k_dusk_exposure_lift = 1.45;
const float k_grain_shadow_bias = 0.65;
const int k_ao_tap_count = 8;
const int k_ao_ring_count = 3;
const float k_ao_ring_spread[3] = float[3](2.5, 8.0, 18.0);
const float k_ao_ring_weight[3] = float[3](1.0, 0.78, 0.46);
const float k_ao_ring_reach = 1.7;
const float k_ao_far_cutoff = 26.0;
const vec3 k_ao_tint = vec3(0.72, 0.76, 0.86);

const float k_fog_horizon_weight = 0.46;
const float k_fog_horizon_gain = 0.48;
const float k_fog_desaturation = 0.22;
const float k_fog_inscatter_power = 5.0;
const float k_fog_inscatter_gain = 0.42;
const vec3 k_fog_haze_tone = vec3(0.86, 0.84, 0.82);

const float k_ground_fog_gain = 0.30;
const float k_ground_fog_night_gain = 0.22;
const float k_ground_fog_dawn_gain = 0.30;
const float k_ground_fog_weather_gain = 0.22;
const float k_ground_fog_max_opacity = 0.45;
const float k_ground_fog_distance_reach = 34.0;
const vec3 k_ground_fog_lift = vec3(1.10, 1.10, 1.08);
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
#if !SOI_SURFACE_DETAIL

  return 0.0;
#else
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
#endif
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

vec3 fog_color_toward(vec3 world) {
  vec3 view_dir = normalize(world - u_camera_position);
  float toward_sun = clamp(dot(view_dir, environment_primary_direction()), 0.0, 1.0);
  float low_sun = 1.0 - smoothstep(0.10, 0.60, environment_primary_direction().y);
  float inscatter = pow(toward_sun, k_fog_inscatter_power) * k_fog_inscatter_gain *
                    (0.35 + 0.65 * low_sun);
  vec3 sun = environment_primary_color() * environment_primary_intensity();
  vec3 haze = environment_fog_color() * k_fog_haze_tone;
  return haze + sun * inscatter;
}

float valley_fog(vec3 world) {
  float strength = u_ground_fog.z;
  if (strength <= 0.0) {
    return 0.0;
  }
  float floor_y = u_ground_fog.x;
  float ceiling_y = max(u_ground_fog.y, floor_y + 0.05);
  float low = 1.0 - smoothstep(floor_y, ceiling_y, world.y);
  low *= low;
  float drift =
      soi_value_noise_e2c097(world.xz * 0.045 + vec2(u_time * 0.030, -u_time * 0.018));
  float swirl =
      soi_value_noise_e2c097(world.xz * 0.14 - vec2(u_time * 0.012, u_time * 0.021));
  float patch = smoothstep(0.28, 0.80, 0.60 * drift + 0.40 * swirl);
  float breakup = 0.30 + 0.70 * patch;
  float view_distance = length(u_camera_position - world);
  float thickness = 1.0 - exp(-view_distance / k_ground_fog_distance_reach);
  float night = environment_night_amount();
  float dawn = environment_low_sun_amount();
  float weather = clamp(environment_fog_density() * 40.0, 0.0, 1.0);
  float time_gain = k_ground_fog_gain + night * k_ground_fog_night_gain +
                    dawn * k_ground_fog_dawn_gain +
                    weather * k_ground_fog_weather_gain;
  return clamp(strength * low * breakup * (0.25 + 0.75 * thickness) * time_gain,
               0.0,
               k_ground_fog_max_opacity);
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

float night_amount() {
  return environment_night_amount();
}

float dusk_amount() {
  vec3 primary = environment_primary_color();
  float warm = smoothstep(0.15, 0.55, primary.r - primary.b);
  float low_sun = 1.0 - smoothstep(0.12, 0.55, environment_primary_direction().y);
  return clamp(warm * low_sun, 0.0, 1.0);
}

vec3 film_grain(vec3 color) {
  float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
  vec2 seed = v_uv * vec2(1917.0, 1081.0) +
              vec2(fract(u_time * 7.31) * 113.0, fract(u_time * 3.17) * 71.0);
  float noise = soi_hash13_a1b3c9(vec3(floor(seed), 1.0)) - 0.5;
  float weight = mix(1.0, 1.0 - luma, k_grain_shadow_bias);
  return color + noise * k_grain_strength * weight;
}

const int k_far_blur_taps = 8;
const float k_far_blur_radius = 2.6;
const float k_far_blur_start = 0.35;
const vec2 k_far_blur_offsets[8] = vec2[8](vec2(1.0, 0.0),
                                           vec2(0.7071, 0.7071),
                                           vec2(0.0, 1.0),
                                           vec2(-0.7071, 0.7071),
                                           vec2(-1.0, 0.0),
                                           vec2(-0.7071, -0.7071),
                                           vec2(0.0, -1.0),
                                           vec2(0.7071, -0.7071));

vec3 far_softened_scene(float fog) {
  vec3 center = texture(u_scene, v_uv).rgb;
#if !SOI_SURFACE_DETAIL
  return center;
#else
  float amount = smoothstep(k_far_blur_start, 1.0, fog);
  if (amount <= 0.0) {
    return center;
  }
  vec3 sum = center;
  float radius = k_far_blur_radius * amount;
  for (int i = 0; i < k_far_blur_taps; ++i) {
    sum +=
        texture(u_scene, v_uv + k_far_blur_offsets[i] * u_inverse_resolution * radius)
            .rgb;
  }
  return mix(center, sum / float(k_far_blur_taps + 1), amount);
#endif
}

void main() {
  float center_depth = linear_depth(v_uv);
  bool has_geometry = center_depth < u_depth_range.y * 0.999;
  vec3 world = has_geometry ? world_position(v_uv) : vec3(0.0);
  float fog = has_geometry ? scene_fog_amount(world) : 0.0;
  vec3 scene = far_softened_scene(fog);
  vec3 bloom = texture(u_bloom, v_uv).rgb;
  vec3 combined = scene + bloom * u_bloom_intensity;
  if (u_godray_strength > 0.0) {
    float rays = texture(u_rays, v_uv).r;
    combined += environment_primary_color() * environment_primary_intensity() *
                (rays * u_godray_strength);
  }

  float occlusion = clamp(grounding_occlusion() * u_ground_ao_strength, 0.0, 1.0);
  combined = mix(combined, combined * k_ao_tint, occlusion);

  if (has_geometry) {
    vec2 mist = ground_mist(world);
    float valley = valley_fog(world);
    combined = mix(combined, fog_color_toward(world) * k_ground_fog_lift, valley);
    combined = mix(combined, environment_fog_color() * k_mist_water_lift, mist.x);
    combined = mix(combined,
                   environment_fog_color() * k_mist_miasma_tint + k_mist_miasma_floor,
                   mist.y);

    float haze_luma = dot(combined, vec3(0.2126, 0.7152, 0.0722));
    combined = mix(combined, vec3(haze_luma), fog * k_fog_desaturation);
    combined = mix(combined, fog_color_toward(world), fog);
  }

  float night = night_amount();
  float dusk = dusk_amount();
  combined *=
      mix(1.0, k_night_exposure_lift, night) * mix(1.0, k_dusk_exposure_lift, dusk);
  vec3 graded = soi_finalize(combined);
  graded = soi_time_of_day_grade(graded, night, dusk);

  vec2 centered = v_uv * 2.0 - 1.0;
  float falloff = smoothstep(k_vignette_inner, k_vignette_outer, length(centered));
  float vignette = 1.0 - falloff * u_vignette_strength;
  graded = mix(graded * k_vignette_tint, graded, vignette);
  graded *= vignette;
  graded = film_grain(graded);

  frag_color = vec4(clamp(graded, 0.0, 1.0), 1.0);
}
