#version 330 core
#include "sky_common.glsl"

in vec3 v_direction;

uniform float u_sky_box_blend;
uniform float u_sky_box_time;

out vec4 frag_color;

#if SOI_QUALITY_TIER >= SOI_TIER_ULTRA
#define SOI_SKY_BOX_STEPS 24
#define SOI_SKY_BOX_LIGHT_STEPS 3
#define SOI_SKY_BOX_LIGHT_STRIDE 1.0
#define SOI_SKY_BOX_FIELD_OCTAVES 4
#define SOI_SKY_BOX_DETAIL_OCTAVES 2
#define SOI_SKY_BOX_CIRRUS 1
#define SOI_SKY_BOX_SOFTEN 1.0
#elif SOI_QUALITY_TIER >= SOI_TIER_HIGH
#define SOI_SKY_BOX_STEPS 18
#define SOI_SKY_BOX_LIGHT_STEPS 2
#define SOI_SKY_BOX_LIGHT_STRIDE 1.8
#define SOI_SKY_BOX_FIELD_OCTAVES 4
#define SOI_SKY_BOX_DETAIL_OCTAVES 2
#define SOI_SKY_BOX_CIRRUS 1
#define SOI_SKY_BOX_SOFTEN 1.0
#elif SOI_QUALITY_TIER >= SOI_TIER_MEDIUM
#define SOI_SKY_BOX_STEPS 12
#define SOI_SKY_BOX_LIGHT_STEPS 2
#define SOI_SKY_BOX_LIGHT_STRIDE 1.8
#define SOI_SKY_BOX_FIELD_OCTAVES 3
#define SOI_SKY_BOX_DETAIL_OCTAVES 1
#define SOI_SKY_BOX_CIRRUS 1

#define SOI_SKY_BOX_SOFTEN 1.7
#else
#define SOI_SKY_BOX_STEPS 8
#define SOI_SKY_BOX_LIGHT_STEPS 1

#define SOI_SKY_BOX_LIGHT_STRIDE 4.7
#define SOI_SKY_BOX_FIELD_OCTAVES 3
#define SOI_SKY_BOX_DETAIL_OCTAVES 1
#define SOI_SKY_BOX_CIRRUS 0
#define SOI_SKY_BOX_SOFTEN 2.2
#endif

const float k_box_planet_radius = 42000.0;
const float k_box_base_altitude = 1200.0;
const float k_box_top_altitude = 1950.0;
const float k_box_slab_span = k_box_top_altitude - k_box_base_altitude;
const float k_box_march_reach = 13000.0;
const float k_box_min_upward = 0.002;
const float k_box_horizon_fade_start = 0.004;
const float k_box_horizon_fade_end = 0.040;
const vec2 k_box_wind = vec2(11.0, 4.5);
const float k_box_field_scale = 0.00080;
const float k_box_edge_scale = 3.7;
const float k_box_edge_weight = 0.24;
const float k_box_detail_scale = 0.0034;
const float k_box_detail_drift = 1.7;
const float k_box_coverage_floor = 0.34;
const float k_box_coverage_gain = 0.50;
const float k_box_coverage_softness = 0.055;
const float k_box_billow_gain = 3.4;
const float k_box_top_minimum = 0.30;
const float k_box_base_round = 0.20;
const float k_box_shoulder = 0.52;
const float k_box_erosion = 0.62;
const float k_box_erosion_floor = 0.55;
const float k_box_density_gain = 2.10;
const float k_box_extinction = 0.0072;
const float k_box_light_step = 55.0;
const float k_box_light_step_growth = 2.10;
const float k_box_light_extinction = 0.0045;
const float k_box_light_skip_transmittance = 0.06;
const float k_box_powder = 2.4;
const float k_box_powder_floor = 0.30;
const float k_box_sun_gain = 1.55;
const float k_box_ambient_gain = 0.50;
const float k_box_base_shade = 0.58;
const float k_box_base_sky_tint = 0.45;
const float k_box_anisotropy_forward = 0.72;
const float k_box_anisotropy_back = 0.25;
const float k_box_phase_forward_mix = 0.62;
const float k_box_phase_weight = 0.50;
const float k_box_phase_min = 0.42;
const float k_box_phase_max = 1.90;
const float k_box_transmittance_cutoff = 0.02;
const float k_box_haze_density = 0.000088;
const float k_box_haze_weather = 6.5;
const float k_box_haze_strength = 0.86;
const float k_box_celestial_occlusion = 0.94;
const float k_box_cirrus_altitude = 5400.0;
const float k_box_cirrus_scale = 0.00030;
const float k_box_cirrus_stretch = 3.6;
const float k_box_cirrus_drift = 2.4;
const float k_box_cirrus_coverage = 0.38;
const float k_box_cirrus_softness = 0.26;
const float k_box_cirrus_opacity = 0.24;
const float k_box_cirrus_sun_gain = 0.35;
const float k_box_dither_x = 0.06711056;
const float k_box_dither_y = 0.00583715;
const float k_box_dither_scale = 52.9829189;

float sky_box_dither(vec2 fragment) {
  float gradient = fract(k_box_dither_scale *
                         fract(dot(fragment, vec2(k_box_dither_x, k_box_dither_y))));
  return fract(gradient + soi_hash12_dbdbc1(fragment));
}

float sky_box_fbm2(vec2 point, int octaves) {
  float value = 0.0;
  float amplitude = 0.5;
  float total = 0.0;
  for (int octave = 0; octave < octaves; ++octave) {
    value += amplitude * soi_noise2(point);
    total += amplitude;
    point = point * 2.07 + vec2(19.7, 11.3);
    amplitude *= 0.5;
  }
  return value / max(total, 1e-4);
}

float sky_box_fbm3(vec3 point, int octaves) {
  float value = 0.0;
  float amplitude = 0.5;
  float total = 0.0;
  for (int octave = 0; octave < octaves; ++octave) {
    value += amplitude * soi_noise3(point);
    total += amplitude;
    point = point * 2.11 + vec3(7.3, 13.9, 5.1);
    amplitude *= 0.5;
  }
  return value / max(total, 1e-4);
}

float sky_box_layer_distance(float upward, float altitude) {
  float along = k_box_planet_radius * upward;
  return sqrt((along * along) + (2.0 * k_box_planet_radius * altitude) +
              (altitude * altitude)) -
         along;
}

float sky_box_density(vec3 position, float coverage) {
  vec2 drift = k_box_wind * u_sky_box_time;
  float height = clamp((position.y - k_box_base_altitude) / k_box_slab_span, 0.0, 1.0);

  vec2 plane = (position.xz + drift) * k_box_field_scale;
  float field = mix(sky_box_fbm2(plane, SOI_SKY_BOX_FIELD_OCTAVES),
                    sky_box_fbm2(plane * k_box_edge_scale + 31.7, 2),
                    k_box_edge_weight);

  float excess = field - (1.0 - coverage);
  float softness = k_box_coverage_softness * SOI_SKY_BOX_SOFTEN;
  float shape = smoothstep(-softness, softness, excess);
  if (shape <= 0.0) {
    return 0.0;
  }

  float billow = clamp(excess * k_box_billow_gain, 0.0, 1.0);
  float top = mix(k_box_top_minimum, 1.0, billow);
  float profile = smoothstep(0.0, k_box_base_round * top, height) *
                  (1.0 - smoothstep(k_box_shoulder * top, top, height));
  if (profile <= 0.0) {
    return 0.0;
  }

  vec3 detail_position = position + vec3(drift.x, 0.0, drift.y) * k_box_detail_drift;
  float detail =
      sky_box_fbm3(detail_position * k_box_detail_scale, SOI_SKY_BOX_DETAIL_OCTAVES);
  float erosion = detail * (k_box_erosion / SOI_SKY_BOX_SOFTEN) *
                  (k_box_erosion_floor + ((1.0 - k_box_erosion_floor) * height));

  float density =
      clamp((shape * profile - erosion) / max(1.0 - erosion, 1e-3), 0.0, 1.0);
  return density * k_box_density_gain;
}

float sky_box_light_transmittance(vec3 position, vec3 sun_direction, float coverage) {
  float optical_depth = 0.0;
  float step_length = k_box_light_step * SOI_SKY_BOX_LIGHT_STRIDE;
  vec3 sample_position = position;
  for (int step_index = 0; step_index < SOI_SKY_BOX_LIGHT_STEPS; ++step_index) {
    sample_position += sun_direction * step_length;
    optical_depth += sky_box_density(sample_position, coverage) * step_length;
    step_length *= k_box_light_step_growth;
  }
  return exp(-optical_depth * k_box_light_extinction);
}

float sky_box_henyey(float cos_angle, float anisotropy) {
  float squared = anisotropy * anisotropy;
  float denominator = 1.0 + squared - (2.0 * anisotropy * cos_angle);
  return (1.0 - squared) / pow(max(denominator, 1e-4), 1.5);
}

float sky_box_phase(float cos_angle) {

  float lobes = mix(sky_box_henyey(cos_angle, -k_box_anisotropy_back),
                    sky_box_henyey(cos_angle, k_box_anisotropy_forward),
                    k_box_phase_forward_mix);
  return clamp(mix(1.0, lobes, k_box_phase_weight), k_box_phase_min, k_box_phase_max);
}

float sky_box_haze(float distance_travelled) {
  float density = k_box_haze_density +
                  (environment_fog_density() * k_box_haze_density * k_box_haze_weather);
  return (1.0 - exp(-max(distance_travelled, 0.0) * density)) * k_box_haze_strength;
}

vec4 sky_box_clouds(vec3 ray, float dither) {
  float cloud_cover = clamp(environment_cloud_cover(), 0.0, 1.0);
  if (cloud_cover <= 0.0001) {
    return vec4(0.0);
  }

  if (ray.y <= k_box_min_upward) {
    return vec4(0.0);
  }

  float near_distance = sky_box_layer_distance(ray.y, k_box_base_altitude);
  float far_distance =
      min(sky_box_layer_distance(ray.y, k_box_top_altitude), k_box_march_reach);
  if (far_distance <= near_distance) {
    return vec4(0.0);
  }

  float coverage =
      clamp(cloud_cover * k_box_coverage_gain + k_box_coverage_floor, 0.0, 1.0);
  vec3 sun_direction = environment_primary_direction();
  vec3 sun_color = environment_primary_color() * environment_primary_intensity();
  vec3 ambient = sky_cloud_tint(ray) * k_box_ambient_gain;
  vec3 ambient_base =
      mix(ambient, environment_sky_color() * k_box_ambient_gain, k_box_base_sky_tint) *
      k_box_base_shade;
  float phase = sky_box_phase(clamp(dot(ray, sun_direction), -1.0, 1.0));

  float step_length = (far_distance - near_distance) / float(SOI_SKY_BOX_STEPS);
  float travelled = near_distance + (step_length * dither);
  float transmittance = 1.0;
  float first_hit = far_distance;
  vec3 scattered = vec3(0.0);

  for (int step_index = 0; step_index < SOI_SKY_BOX_STEPS; ++step_index) {
    vec3 position = ray * travelled;
    float density = sky_box_density(position, coverage);
    if (density > 0.0) {
      first_hit = min(first_hit, travelled);
      float sun_visibility =
          transmittance > k_box_light_skip_transmittance
              ? sky_box_light_transmittance(position, sun_direction, coverage)
              : 0.0;
      float powder = k_box_powder_floor +
                     (1.0 - k_box_powder_floor) * (1.0 - exp(-density * k_box_powder));
      float height =
          clamp((position.y - k_box_base_altitude) / k_box_slab_span, 0.0, 1.0);
      vec3 luminance = mix(ambient_base, ambient, height) +
                       sun_color * sun_visibility * phase * powder * k_box_sun_gain;
      float sample_transmittance = exp(-density * step_length * k_box_extinction);
      scattered += transmittance * (1.0 - sample_transmittance) * luminance;
      transmittance *= sample_transmittance;
      if (transmittance < k_box_transmittance_cutoff) {
        break;
      }
    }
    travelled += step_length;
  }

  float alpha = 1.0 - transmittance;

  scattered = mix(scattered, sky_gradient(ray) * alpha, sky_box_haze(first_hit));

  float horizon_fade =
      smoothstep(k_box_horizon_fade_start, k_box_horizon_fade_end, ray.y);
  return vec4(scattered * horizon_fade, alpha * horizon_fade);
}

#if SOI_SKY_BOX_CIRRUS
vec4 sky_box_cirrus(vec3 ray) {
  if (ray.y <= k_box_min_upward) {
    return vec4(0.0);
  }

  float distance_to_layer = sky_box_layer_distance(ray.y, k_box_cirrus_altitude);
  vec2 plane = ((ray.xz * distance_to_layer) +
                (k_box_wind * u_sky_box_time * k_box_cirrus_drift)) *
               k_box_cirrus_scale;
  plane.x /= k_box_cirrus_stretch;

  float coverage =
      clamp(environment_cloud_cover() * k_box_coverage_gain + k_box_cirrus_coverage,
            0.0,
            1.0);
  float field = sky_box_fbm2(plane, SOI_SKY_BOX_FIELD_OCTAVES);
  float veil = smoothstep(1.0 - coverage - k_box_cirrus_softness,
                          1.0 - coverage + k_box_cirrus_softness,
                          field);
  if (veil <= 0.0) {
    return vec4(0.0);
  }

  float toward_sun =
      clamp(dot(ray, environment_primary_direction()) * 0.5 + 0.5, 0.0, 1.0);
  vec3 tint = sky_cloud_tint(ray) *
              (1.0 + k_box_cirrus_sun_gain * pow(toward_sun, 4.0)) *
              max(environment_primary_intensity(), 0.35);
  float alpha = veil * k_box_cirrus_opacity *
                smoothstep(k_box_horizon_fade_start, k_box_horizon_fade_end, ray.y);
  vec3 color =
      mix(tint * alpha, sky_gradient(ray) * alpha, sky_box_haze(distance_to_layer));
  return vec4(color, alpha);
}
#endif

void main() {
  vec3 ray = normalize(v_direction);
  vec3 sky = sky_gradient(ray);

#if SOI_SKY_BOX_CIRRUS
  vec4 cirrus = sky_box_cirrus(ray);
  sky = sky * (1.0 - cirrus.a) + cirrus.rgb;
#else
  vec4 cirrus = vec4(0.0);
#endif

  float dither = sky_box_dither(gl_FragCoord.xy);
  vec4 clouds = sky_box_clouds(ray, dither);
  sky = sky * (1.0 - clouds.a) + clouds.rgb;

  float occlusion = clamp(clouds.a + cirrus.a * 0.5, 0.0, 1.0);
  sky += sky_celestial(ray, occlusion) * (1.0 - occlusion * k_box_celestial_occlusion);

  frag_color = vec4(sky * environment_exposure(), u_sky_box_blend);
}
