#version 330 core
#include "environment_lighting.glsl"
#include "noise.glsl"

in vec2 v_uv;

uniform mat4 u_inverse_view_proj;

out vec4 frag_color;

const float k_sky_zenith_gain = 1.06;
const float k_sky_horizon_lift = 1.22;
const vec3 k_sky_zenith_deepen = vec3(0.70, 0.83, 1.00);
const float k_sky_horizon_blend = 2.20;
const float k_sky_band_scale = 0.55;
const float k_sky_cloud_plane_lift = 0.12;
const float k_sky_band_softness = 0.34;
const float k_sky_cloud_floor = 0.18;
const float k_sky_sun_tightness = 220.0;
const float k_sky_sun_halo = 6.0;
const vec3 k_sky_cloud_color = vec3(1.02, 1.00, 0.97);
const float k_sky_sun_disc_cos = 0.9993;
const float k_sky_sun_disc_soft = 0.0006;
const float k_sky_sun_disc_gain = 6.0;
const float k_sky_dusk_band_gain = 0.85;
const float k_sky_dusk_band_reach = 3.5;
const vec3 k_sky_dusk_band_tone = vec3(1.20, 0.72, 0.42);
const float k_sky_moon_disc_cos = 0.99955;
const float k_sky_moon_disc_soft = 0.0004;
const vec3 k_sky_moon_color = vec3(0.92, 0.95, 1.05);
const float k_sky_moon_gain = 2.2;
const float k_sky_moon_halo = 14.0;
const float k_sky_star_density = 0.985;
const float k_sky_star_gain = 0.55;
const float k_sky_star_scale = 260.0;

vec3 sky_ray_direction() {
  vec2 ndc = v_uv * 2.0 - 1.0;
  vec4 near_point = u_inverse_view_proj * vec4(ndc, -1.0, 1.0);
  vec4 far_point = u_inverse_view_proj * vec4(ndc, 1.0, 1.0);
  vec3 near_world = near_point.xyz / near_point.w;
  vec3 far_world = far_point.xyz / far_point.w;
  return normalize(far_world - near_world);
}

vec3 sky_stars(vec3 ray, float night) {
  if (night <= 0.0 || ray.y <= 0.02) {
    return vec3(0.0);
  }
  vec3 cell = floor(ray * k_sky_star_scale);
  float seed = soi_hash13_a1b3c9(cell);
  float star = smoothstep(k_sky_star_density, 1.0, seed);
  vec3 within = fract(ray * k_sky_star_scale) - 0.5;
  float point = 1.0 - smoothstep(0.0, 0.32, length(within));
  float twinkle = 0.7 + 0.3 * soi_hash13_a1b3c9(cell + 17.0);
  float horizon_fade = smoothstep(0.02, 0.30, ray.y);
  return vec3(star * point * twinkle * k_sky_star_gain * night * horizon_fade);
}

void main() {
  vec3 ray = sky_ray_direction();
  float upward = clamp(ray.y, 0.0, 1.0);
  float gradient = pow(upward, 1.0 / k_sky_horizon_blend);

  vec3 horizon = environment_fog_color() * k_sky_horizon_lift;
  vec3 zenith = environment_sky_color() * k_sky_zenith_gain * k_sky_zenith_deepen;
  vec3 sky = mix(horizon, zenith, gradient);

  vec3 sun_direction = environment_primary_direction();
  vec3 sun_color = environment_primary_color() * environment_primary_intensity();
  float moonlit = environment_night_amount();
  float low_sun = environment_low_sun_amount();

  float toward_sun = clamp(dot(ray, sun_direction), 0.0, 1.0);
  vec2 sun_flat = normalize(sun_direction.xz + vec2(1e-4, 0.0));
  vec2 ray_flat = normalize(ray.xz + vec2(1e-4, 0.0));
  float toward_sun_azimuth = clamp(dot(ray_flat, sun_flat) * 0.5 + 0.5, 0.0, 1.0);
  float band_height = exp(-upward * k_sky_dusk_band_reach);
  float dusk_band = low_sun * band_height * pow(toward_sun_azimuth, 2.5);
  sky = mix(sky,
            environment_primary_color() * k_sky_dusk_band_tone *
                max(environment_primary_intensity(), 0.35),
            dusk_band * k_sky_dusk_band_gain);

  vec2 cloud_plane = ray.xz / (upward + k_sky_cloud_plane_lift);
  float band_field = soi_fbm_23e5ab(cloud_plane * k_sky_band_scale);
  band_field =
      mix(band_field, soi_fbm_23e5ab(cloud_plane * k_sky_band_scale * 3.1 + 7.3), 0.30);
  float coverage = clamp(environment_cloud_cover() + k_sky_cloud_floor, 0.0, 1.0);
  float bands = smoothstep(1.0 - coverage - k_sky_band_softness,
                           1.0 - coverage + k_sky_band_softness,
                           band_field);
  bands *= smoothstep(0.02, 0.30, upward);
  vec3 cloud_lit = horizon * k_sky_cloud_color;
  cloud_lit = mix(cloud_lit,
                  cloud_lit * k_sky_dusk_band_tone,
                  low_sun * pow(toward_sun_azimuth, 1.5) * 0.7);
  sky = mix(sky, cloud_lit, bands * 0.72);

  sky += sun_color * pow(toward_sun, k_sky_sun_tightness) * 0.85 * (1.0 - moonlit);
  sky += sun_color * pow(toward_sun, k_sky_sun_halo) * (0.09 + 0.14 * low_sun);
  float sun_disc = smoothstep(k_sky_sun_disc_cos - k_sky_sun_disc_soft,
                              k_sky_sun_disc_cos + k_sky_sun_disc_soft,
                              toward_sun);
  sky += sun_color * sun_disc * k_sky_sun_disc_gain * (1.0 - moonlit);

  float moon_disc = smoothstep(k_sky_moon_disc_cos - k_sky_moon_disc_soft,
                               k_sky_moon_disc_cos + k_sky_moon_disc_soft,
                               toward_sun);
  float moon_halo = pow(toward_sun, k_sky_moon_halo);
  sky += k_sky_moon_color * (moon_disc * k_sky_moon_gain + moon_halo * 0.10) * moonlit;
  sky += sky_stars(ray, moonlit) * (1.0 - bands);

  frag_color = vec4(sky * environment_exposure(), 1.0);
}
