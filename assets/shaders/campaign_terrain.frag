#version 330 core
#include "noise.glsl"

in vec2 v_uv;
in vec3 v_normal;
in vec3 v_world_pos;
in float v_height;

uniform sampler2D u_base_texture;
uniform sampler2D u_hillshade_texture;
uniform sampler2D u_parchment_texture;

uniform vec3 u_light_direction;
uniform float u_ambient_strength;
uniform float u_hillshade_strength;
uniform float u_ao_strength;

uniform bool u_use_hillshade;
uniform bool u_use_parchment;
uniform bool u_use_lighting;

uniform vec3 u_water_deep_color;
uniform vec3 u_water_shallow_color;
uniform vec3 u_lowland_tint;
uniform vec3 u_highland_tint;
uniform vec3 u_mountain_tint;

uniform float u_elevation_scale;

out vec4 frag_color;

float compute_ao(vec3 normal) {

  float ao = clamp(normal.y * 0.5 + 0.5, 0.0, 1.0);

  return mix(1.0, ao, u_ao_strength);
}

float compute_hillshade(vec3 normal, vec3 light_dir) {
  float ndotl = max(0.0, dot(normal, light_dir));
  return u_ambient_strength + (1.0 - u_ambient_strength) * ndotl;
}

vec3 get_elevation_tint(float height) {
  if (height < 0.0) {
    float depth = clamp(-height, 0.0, 1.0);
    return mix(u_water_shallow_color, u_water_deep_color, depth);
  }

  float h = clamp(height, 0.0, 1.0);
  if (h < 0.14) {

    return mix(vec3(1.0), u_lowland_tint, smoothstep(0.0, 0.14, h));
  }
  if (h < 0.38) {
    float t = smoothstep(0.14, 0.38, h);
    return mix(u_lowland_tint, u_highland_tint, t);
  }
  float t = smoothstep(0.38, 0.85, h);
  return mix(u_highland_tint, u_mountain_tint, t);
}

float land_at(vec2 uv) {
  vec4 c = textureLod(u_base_texture, vec2(uv.x, 1.0 - uv.y), 0.0);
  return smoothstep(-0.01, 0.05, c.r - c.b);
}

float get_parchment_pattern(vec2 uv) {

  float n1 = soi_fbm_d6cc9d(uv * 8.0, 3);
  float n2 = soi_fbm_d6cc9d(uv * 20.0 + vec2(100.0), 2);

  float pattern = n1 * 0.6 + n2 * 0.4;

  return 0.955 + pattern * 0.045;
}

void main() {

  vec2 uv = vec2(v_uv.x, 1.0 - v_uv.y);
  vec4 base_color = texture(u_base_texture, uv);

  vec3 color = base_color.rgb;

  float land = land_at(v_uv);
  color *= mix(vec3(1.0), get_elevation_tint(v_height * u_elevation_scale), land);

  if (u_use_hillshade || u_use_lighting) {
    vec3 light_dir = normalize(u_light_direction);
    float ndotl = max(0.0, dot(v_normal, light_dir));
    float shade = u_ambient_strength + (1.0 - u_ambient_strength) * ndotl;
    shade *= compute_ao(v_normal);

    float relief = 1.0 + (shade - 1.0) * u_hillshade_strength;
    color *= mix(1.0, relief, land);
  }

  if (u_use_parchment) {
    float parchment = get_parchment_pattern(v_uv);
    color *= mix(1.0, parchment, land);
    color = mix(color, color * vec3(1.02, 1.0, 0.96), 0.3);
  }

  vec2 pixel = fwidth(v_uv);

  vec2 ink_step = pixel * 1.7;
  float offshore = min(
      min(land_at(v_uv + vec2(ink_step.x, 0.0)), land_at(v_uv - vec2(ink_step.x, 0.0))),
      min(land_at(v_uv + vec2(0.0, ink_step.y)),
          land_at(v_uv - vec2(0.0, ink_step.y))));
  float coast = land * (1.0 - offshore);
  color = mix(color, color * vec3(0.34, 0.31, 0.28), coast);

  vec2 halo_step = pixel * 5.0;
  float ashore =
      land_at(v_uv + vec2(halo_step.x, 0.0)) + land_at(v_uv - vec2(halo_step.x, 0.0)) +
      land_at(v_uv + vec2(0.0, halo_step.y)) + land_at(v_uv - vec2(0.0, halo_step.y)) +
      land_at(v_uv + halo_step) + land_at(v_uv - halo_step) +
      land_at(v_uv + vec2(halo_step.x, -halo_step.y)) +
      land_at(v_uv - vec2(halo_step.x, -halo_step.y));

  float shallows = (1.0 - land) * smoothstep(0.18, 0.62, ashore * 0.125);
  color = mix(color, mix(color, vec3(0.55, 0.68, 0.74), 0.5), shallows);

  vec2 vignette_coord = v_uv * 2.0 - 1.0;
  float vignette = 1.0 - smoothstep(0.75, 1.5, length(vignette_coord)) * 0.06;
  color *= vignette;

  frag_color = vec4(color, base_color.a * max(land, shallows));
}
