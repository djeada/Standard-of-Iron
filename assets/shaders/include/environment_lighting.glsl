#include "tonemap.glsl"
layout(std140) uniform EnvironmentLighting {
  vec4 u_env_primary_direction_intensity;
  vec4 u_env_primary_color_ambient_intensity;
  vec4 u_env_sky_color_exposure;
  vec4 u_env_ground_bounce_color_fog_density;
  vec4 u_env_fog_color_cloud_cover;
  vec4 u_env_shadow_tint_strength;
  vec4 u_env_shadow_softness_wetness;
};

vec3 environment_primary_direction() {
  return normalize(u_env_primary_direction_intensity.xyz);
}

vec3 environment_primary_color() {
  return u_env_primary_color_ambient_intensity.rgb;
}

float environment_primary_intensity() {
  return u_env_primary_direction_intensity.w;
}

float environment_ambient_intensity() {
  return u_env_primary_color_ambient_intensity.w;
}

vec3 environment_sky_color() {
  return u_env_sky_color_exposure.rgb;
}

vec3 environment_ground_bounce_color() {
  return u_env_ground_bounce_color_fog_density.rgb;
}

float environment_exposure() {
  return u_env_sky_color_exposure.w;
}

vec3 environment_fog_color() {
  return u_env_fog_color_cloud_cover.rgb;
}

float environment_fog_density() {
  return u_env_ground_bounce_color_fog_density.w;
}

float environment_cloud_cover() {
  return u_env_fog_color_cloud_cover.w;
}

vec3 environment_shadow_tint() {
  return u_env_shadow_tint_strength.rgb;
}

float environment_shadow_strength() {
  return u_env_shadow_tint_strength.w;
}

float environment_shadow_softness() {
  return u_env_shadow_softness_wetness.x;
}

float environment_wetness() {
  return u_env_shadow_softness_wetness.y;
}

vec3 environment_ambient_light(vec3 normal) {
  float hemisphere = clamp(normal.y * 0.5 + 0.5, 0.0, 1.0);
  return mix(environment_ground_bounce_color(), environment_sky_color(), hemisphere) *
         environment_ambient_intensity();
}

const float k_soi_shade_wrap = 0.28;
const float k_soi_shade_band_count = 3.0;
const float k_soi_shade_band_softness = 0.22;
const float k_soi_shade_band_mix = 0.30;
const float k_soi_shade_rim_power = 2.60;
const float k_soi_shade_rim_strength = 0.16;

float soi_soft_band(float value) {
  float scaled = value * k_soi_shade_band_count;
  float plateau = floor(scaled);
  float within = fract(scaled);
  float stepped = plateau + smoothstep(0.5 - k_soi_shade_band_softness,
                                       0.5 + k_soi_shade_band_softness,
                                       within);
  return clamp(stepped / k_soi_shade_band_count, 0.0, 1.0);
}

float soi_wrapped_diffuse(vec3 normal) {
  float ndl = dot(normal, environment_primary_direction());
  float wrapped = clamp((ndl + k_soi_shade_wrap) / (1.0 + k_soi_shade_wrap), 0.0, 1.0);
  return mix(wrapped, soi_soft_band(wrapped), k_soi_shade_band_mix);
}

vec3 soi_key_light(vec3 normal) {
  return environment_primary_color() * environment_primary_intensity() *
         soi_wrapped_diffuse(normal);
}

vec3 soi_surface_lighting_scaled(vec3 normal, float key_scale) {
  return (environment_ambient_light(normal) + soi_key_light(normal) * key_scale) *
         environment_exposure();
}

vec3 soi_surface_lighting(vec3 normal) {
  return soi_surface_lighting_scaled(normal, 1.0);
}

float soi_rim_term(vec3 normal, vec3 view_dir) {
  return pow(1.0 - clamp(dot(normal, view_dir), 0.0, 1.0), k_soi_shade_rim_power);
}

vec3 soi_rim_light(vec3 normal, vec3 view_dir) {
  float facing = 0.35 + 0.65 * soi_wrapped_diffuse(normal);
  return environment_sky_color() * soi_rim_term(normal, view_dir) *
         k_soi_shade_rim_strength * facing;
}

vec3 environment_direct_light(vec3 normal, float wrap) {
  float wrapped = clamp(
      (dot(normal, environment_primary_direction()) + wrap) / (1.0 + wrap), 0.0, 1.0);
  return environment_primary_color() * environment_primary_intensity() * wrapped;
}

vec3 environment_lighting(vec3 normal, float wrap) {
  return soi_surface_lighting(normal);
}

const float k_fog_reference_span = 88.0;

float atmospheric_fog_amount(float view_distance,
                             float fog_start,
                             float fog_end,
                             float horizon_weight,
                             float horizon_gain) {
  float fog_span = max(fog_end - fog_start, 1e-4);
  float fog_depth = max(view_distance - fog_start, 0.0);
  float normalized_depth = clamp(fog_depth / fog_span, 0.0, 1.0);

  float ranged =
      smoothstep(0.0, 1.0, normalized_depth) * (horizon_weight + horizon_gain);
  float weather =
      1.0 - exp(-environment_fog_density() * normalized_depth * k_fog_reference_span);
  return clamp(max(ranged, weather), 0.0, 1.0);
}
