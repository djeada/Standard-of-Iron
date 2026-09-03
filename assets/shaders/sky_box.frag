#version 330 core
#include "sky_common.glsl"

in vec3 v_direction;

uniform float u_sky_box_blend;
uniform float u_sky_box_time;

out vec4 frag_color;

#if SOI_QUALITY_TIER >= SOI_TIER_ULTRA
#define SOI_SKY_BOX_STEPS 24
#define SOI_SKY_BOX_LIGHT_STEPS 3
#elif SOI_QUALITY_TIER >= SOI_TIER_HIGH
#define SOI_SKY_BOX_STEPS 18
#define SOI_SKY_BOX_LIGHT_STEPS 2
#elif SOI_QUALITY_TIER >= SOI_TIER_MEDIUM
#define SOI_SKY_BOX_STEPS 12
#define SOI_SKY_BOX_LIGHT_STEPS 2
#else
#define SOI_SKY_BOX_STEPS 8
#define SOI_SKY_BOX_LIGHT_STEPS 1
#endif

const float k_box_slab_base = 900.0;
const float k_box_slab_top = 1320.0;
const float k_box_slab_span = k_box_slab_top - k_box_slab_base;
const float k_box_march_reach = 12000.0;
const float k_box_max_step = 110.0;
const float k_box_min_upward = 0.035;
const float k_box_horizon_fade_start = 0.060;
const float k_box_horizon_fade_end = 0.34;
const vec2 k_box_wind = vec2(11.0, 4.5);
const float k_box_field_scale = 0.0010;
const float k_box_edge_scale = 3.7;
const int k_box_edge_octaves = 2;
const float k_box_edge_weight = 0.22;
const float k_box_detail_scale = 0.0030;
const float k_box_detail_drift = 1.7;
const float k_box_coverage_floor = 0.0;
const float k_box_coverage_gain = 0.68;
const float k_box_coverage_softness = 0.12;
const float k_box_profile_base_soft = 0.15;
const float k_box_profile_top_soft = 0.62;
const float k_box_erosion = 0.50;
const float k_box_density_gain = 1.50;
const float k_box_extinction = 0.0080;
const float k_box_light_step = 90.0;
const float k_box_light_extinction = 0.0045;
const float k_box_powder = 1.8;
const float k_box_powder_floor = 0.24;
const float k_box_sun_gain = 0.82;
const float k_box_ambient_gain = 0.34;
const float k_box_anisotropy = 0.55;
const float k_box_phase_weight = 0.32;
const float k_box_phase_min = 0.35;
const float k_box_phase_max = 1.50;
const float k_box_transmittance_cutoff = 0.02;
const float k_box_dither_x = 0.06711056;
const float k_box_dither_y = 0.00583715;
const float k_box_dither_scale = 52.9829189;

float sky_box_dither(vec2 fragment) {
  return fract(k_box_dither_scale *
               fract(dot(fragment, vec2(k_box_dither_x, k_box_dither_y))));
}

float sky_box_density(vec3 position, float coverage) {
  vec2 drift = k_box_wind * u_sky_box_time;
  float height = clamp((position.y - k_box_slab_base) / k_box_slab_span, 0.0, 1.0);
  float profile = smoothstep(0.0, k_box_profile_base_soft, height) *
                  (1.0 - smoothstep(k_box_profile_top_soft, 1.0, height));
  if (profile <= 0.0) {
    return 0.0;
  }

  vec2 plane = (position.xz + drift) * k_box_field_scale;
  float field = mix(soi_fbm_23e5ab(plane),
                    soi_fbm_d6cc9d(plane * k_box_edge_scale, k_box_edge_octaves),
                    k_box_edge_weight);
  float shape = smoothstep(1.0 - coverage - k_box_coverage_softness,
                           1.0 - coverage + k_box_coverage_softness,
                           field);
  float density = shape * profile;
  if (density <= 0.0) {
    return 0.0;
  }

  vec3 detail_position = position + vec3(drift.x, 0.0, drift.y) * k_box_detail_drift;
  float detail = soi_noise3(detail_position * k_box_detail_scale);
  density -= detail * k_box_erosion * height;
  return max(density, 0.0) * k_box_density_gain;
}

float sky_box_light_transmittance(vec3 position, vec3 sun_direction, float coverage) {
  float optical_depth = 0.0;
  vec3 step_vector = sun_direction * k_box_light_step;
  vec3 sample_position = position;
  for (int step_index = 0; step_index < SOI_SKY_BOX_LIGHT_STEPS; ++step_index) {
    sample_position += step_vector;
    optical_depth += sky_box_density(sample_position, coverage) * k_box_light_step;
  }
  return exp(-optical_depth * k_box_light_extinction);
}

float sky_box_phase(float cos_angle) {
  float squared = k_box_anisotropy * k_box_anisotropy;
  float denominator = 1.0 + squared - 2.0 * k_box_anisotropy * cos_angle;
  float henyey_greenstein = (1.0 - squared) / pow(max(denominator, 1e-4), 1.5);
  return clamp(mix(1.0, henyey_greenstein, k_box_phase_weight),
               k_box_phase_min,
               k_box_phase_max);
}

vec4 sky_box_clouds(vec3 ray, float dither) {
  float cloud_cover = clamp(environment_cloud_cover(), 0.0, 1.0);
  if (cloud_cover <= 0.0001) {
    return vec4(0.0);
  }

  if (ray.y <= k_box_min_upward) {
    return vec4(0.0);
  }

  float near_distance = k_box_slab_base / ray.y;
  float far_distance = min(k_box_slab_top / ray.y, k_box_march_reach);
  if (far_distance <= near_distance) {
    return vec4(0.0);
  }

  float coverage =
      clamp(cloud_cover * k_box_coverage_gain + k_box_coverage_floor, 0.0, 1.0);
  vec3 sun_direction = environment_primary_direction();
  vec3 sun_color = environment_primary_color() * environment_primary_intensity();
  vec3 ambient = sky_cloud_tint(ray);
  float phase = sky_box_phase(clamp(dot(ray, sun_direction), -1.0, 1.0));

  float step_length =
      min((far_distance - near_distance) / float(SOI_SKY_BOX_STEPS), k_box_max_step);
  float travelled = near_distance + step_length * dither;
  float transmittance = 1.0;
  vec3 scattered = vec3(0.0);

  for (int step_index = 0; step_index < SOI_SKY_BOX_STEPS; ++step_index) {
    vec3 position = ray * travelled;
    float density = sky_box_density(position, coverage);
    if (density > 0.0) {
      float sun_visibility =
          sky_box_light_transmittance(position, sun_direction, coverage);
      float powder = k_box_powder_floor +
                     (1.0 - k_box_powder_floor) * (1.0 - exp(-density * k_box_powder));
      vec3 luminance = ambient * k_box_ambient_gain +
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

  float horizon_fade =
      smoothstep(k_box_horizon_fade_start, k_box_horizon_fade_end, ray.y);
  return vec4(scattered * horizon_fade, (1.0 - transmittance) * horizon_fade);
}

void main() {
  vec3 ray = normalize(v_direction);
  vec3 sky = sky_gradient(ray);

  float dither = sky_box_dither(gl_FragCoord.xy);
  vec4 clouds = sky_box_clouds(ray, dither);
  sky = sky * (1.0 - clouds.a) + clouds.rgb;
  sky += sky_celestial(ray, clouds.a);

  frag_color = vec4(sky * environment_exposure(), u_sky_box_blend);
}
