#include "quality.glsl"
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

  vec3 primary = environment_primary_color();
  float daylight = 1.0 - smoothstep(0.05, 0.40, primary.b - primary.r);
  return mix(u_env_shadow_tint_strength.rgb, vec3(1.0), 0.20 * daylight);
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

float environment_night_amount() {
  vec3 primary = environment_primary_color();
  return smoothstep(0.05, 0.40, primary.b - primary.r);
}

float environment_darkness_amount() {
  float dim = 1.0 - smoothstep(0.30, 0.62, environment_primary_intensity());
  return max(environment_night_amount(), dim);
}

float environment_low_sun_amount() {
  return (1.0 - smoothstep(0.08, 0.50, environment_primary_direction().y)) *
         (1.0 - environment_night_amount());
}

const float k_horizon_sky_share = 0.42;

vec3 environment_horizon_haze() {
  return mix(environment_fog_color(), environment_sky_color(), k_horizon_sky_share);
}

float environment_cloud_time() {
  return u_env_shadow_softness_wetness.z;
}

float soi_cloud_hash(vec2 cell) {
  return fract(sin(dot(cell, vec2(127.1, 311.7))) * 43758.5453);
}

float soi_cloud_value_noise(vec2 p) {
  vec2 cell = floor(p);
  vec2 f = fract(p);
  vec2 u = f * f * (3.0 - 2.0 * f);
  float a = soi_cloud_hash(cell);
  float b = soi_cloud_hash(cell + vec2(1.0, 0.0));
  float c = soi_cloud_hash(cell + vec2(0.0, 1.0));
  float d = soi_cloud_hash(cell + vec2(1.0, 1.0));
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

const vec2 k_cloud_wind = vec2(1.15, 0.55);
const float k_cloud_scale = 0.0105;
const float k_cloud_clear_strength = 0.24;
const float k_cloud_overcast_strength = 0.46;

float environment_cloud_shadow_amount(vec3 world_position) {
  float daylight = 1.0 - environment_night_amount();
  if (daylight <= 0.0) {
    return 0.0;
  }
  float cover = environment_cloud_cover();
  vec2 p =
      (world_position.xz + k_cloud_wind * environment_cloud_time()) * k_cloud_scale;
  float n = soi_cloud_value_noise(p) * 0.56 +
            soi_cloud_value_noise(p * 2.07 + vec2(17.3, -4.1)) * 0.29 +
            soi_cloud_value_noise(p * 4.31 + vec2(-9.7, 23.5)) * 0.15;
  float threshold = mix(0.60, 0.40, cover);
  float mask = smoothstep(threshold, threshold + 0.17, n);
  float strength = mix(k_cloud_clear_strength, k_cloud_overcast_strength, cover);
  return mask * strength * daylight;
}

const float k_soi_sun_bounce_gain = 0.50;
const float k_soi_horizon_fill_gain = 1.00;

vec3 environment_sun_bounce() {
  float sun_height = clamp(environment_primary_direction().y, 0.0, 1.0);
  return environment_ground_bounce_color() * environment_primary_color() *
         (environment_primary_intensity() * sun_height * k_soi_sun_bounce_gain);
}

vec3 environment_ambient_light(vec3 normal) {
  float hemisphere = clamp(normal.y * 0.5 + 0.5, 0.0, 1.0);
  vec3 dome =
      mix(environment_ground_bounce_color(), environment_sky_color(), hemisphere) *
      environment_ambient_intensity();
  float horizon_view = 1.0 - abs(normal.y);
  vec3 horizon = environment_sky_color() * environment_ambient_intensity() *
                 (horizon_view * k_soi_horizon_fill_gain);
  return dome + horizon + environment_sun_bounce() * (1.0 - hemisphere);
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

const float k_soi_canopy_scatter_gain = 0.34;

vec3 soi_canopy_scatter(vec3 normal) {
  float shaded = 1.0 - soi_wrapped_diffuse(normal);
  return environment_primary_color() * environment_primary_intensity() *
         (shaded * k_soi_canopy_scatter_gain);
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
