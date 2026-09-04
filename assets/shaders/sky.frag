#version 330 core
#include "sky_common.glsl"

in vec2 v_uv;

uniform mat4 u_inverse_view_proj;

out vec4 frag_color;

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
  vec3 sky = sky_gradient(ray);

#if SOI_SURFACE_DETAIL
  vec2 cloud_plane = ray.xz / (upward + k_sky_cloud_plane_lift);
  float band_field = soi_fbm_23e5ab(cloud_plane * k_sky_band_scale);
  band_field =
      mix(band_field, soi_fbm_23e5ab(cloud_plane * k_sky_band_scale * 3.1 + 7.3), 0.30);
  float coverage = clamp(environment_cloud_cover() + k_sky_cloud_floor, 0.0, 1.0);
  float bands = smoothstep(1.0 - coverage - k_sky_band_softness,
                           1.0 - coverage + k_sky_band_softness,
                           band_field);
  bands *= smoothstep(0.02, 0.30, upward);
  sky = mix(sky, sky_cloud_tint(ray), bands * 0.72);
#else
  float bands = clamp(environment_cloud_cover() + k_sky_cloud_floor, 0.0, 1.0) *
                smoothstep(0.02, 0.30, upward);
  sky = mix(sky, sky_cloud_tint(ray), bands * 0.42);
#endif

  sky += sky_celestial(ray, bands);

  frag_color = vec4(sky * environment_exposure(), 1.0);
}
