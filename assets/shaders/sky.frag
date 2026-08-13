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
const float k_sky_band_scale = 2.60;
const float k_sky_band_softness = 0.34;
const float k_sky_cloud_floor = 0.18;
const float k_sky_sun_tightness = 220.0;
const float k_sky_sun_halo = 6.0;
const vec3 k_sky_cloud_color = vec3(1.02, 1.00, 0.97);

vec3 sky_ray_direction() {
  vec2 ndc = v_uv * 2.0 - 1.0;
  vec4 near_point = u_inverse_view_proj * vec4(ndc, -1.0, 1.0);
  vec4 far_point = u_inverse_view_proj * vec4(ndc, 1.0, 1.0);
  vec3 near_world = near_point.xyz / near_point.w;
  vec3 far_world = far_point.xyz / far_point.w;
  return normalize(far_world - near_world);
}

void main() {
  vec3 ray = sky_ray_direction();
  float upward = clamp(ray.y, 0.0, 1.0);
  float gradient = pow(upward, 1.0 / k_sky_horizon_blend);

  vec3 horizon = environment_fog_color() * k_sky_horizon_lift;
  vec3 zenith = environment_sky_color() * k_sky_zenith_gain * k_sky_zenith_deepen;
  vec3 sky = mix(horizon, zenith, gradient);

  float horizontal = max(length(ray.xz), 1e-3);
  vec2 dome = ray.xz / horizontal * (1.0 - upward);
  float band_field = soi_fbm_23e5ab(dome * k_sky_band_scale);
  float coverage = clamp(environment_cloud_cover() + k_sky_cloud_floor, 0.0, 1.0);
  float bands = smoothstep(1.0 - coverage - k_sky_band_softness,
                           1.0 - coverage + k_sky_band_softness,
                           band_field);
  bands *= smoothstep(0.02, 0.42, upward);
  sky = mix(sky, horizon * k_sky_cloud_color, bands * 0.72);

  vec3 sun_direction = environment_primary_direction();
  float toward_sun = clamp(dot(ray, sun_direction), 0.0, 1.0);
  vec3 sun_color = environment_primary_color() * environment_primary_intensity();
  sky += sun_color * pow(toward_sun, k_sky_sun_tightness) * 0.85;
  sky += sun_color * pow(toward_sun, k_sky_sun_halo) * 0.09;

  frag_color = vec4(sky * environment_exposure(), 1.0);
}
