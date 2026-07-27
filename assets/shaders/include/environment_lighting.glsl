// Shared std140 contract. Keep in lockstep with
// Render::EnvironmentLightingState::packed_std140().
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
  return mix(environment_ground_bounce_color(),
             environment_sky_color(),
             hemisphere) *
         environment_ambient_intensity();
}

vec3 environment_direct_light(vec3 normal, float wrap) {
  float wrapped =
      clamp((dot(normal, environment_primary_direction()) + wrap) / (1.0 + wrap),
            0.0,
            1.0);
  return environment_primary_color() * environment_primary_intensity() * wrapped;
}

vec3 environment_lighting(vec3 normal, float wrap) {
  return (environment_ambient_light(normal) +
          environment_direct_light(normal, wrap)) *
         environment_exposure();
}
