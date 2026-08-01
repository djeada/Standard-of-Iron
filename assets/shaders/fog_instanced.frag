#version 330 core
#include "visibility_mask.glsl"

in vec3 v_world_pos;
in vec3 v_color;
in float v_alpha;

out vec4 frag_color;

uniform float u_time;

uniform sampler2D u_fog_mask_tex;
uniform vec2 u_fog_mask_size;
uniform float u_fog_mask_tile_size;
uniform int u_has_fog_mask;

float hash21(vec2 p) {
  p = fract(p * vec2(123.34, 456.21));
  p += dot(p, p + 78.233);
  return fract(p.x * p.y);
}

float vnoise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  vec2 u = f * f * (3.0 - 2.0 * f);

  float a = hash21(i + vec2(0.0, 0.0));
  float b = hash21(i + vec2(1.0, 0.0));
  float c = hash21(i + vec2(0.0, 1.0));
  float d = hash21(i + vec2(1.0, 1.0));

  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

void main() {
  if (u_has_fog_mask != 1 || u_fog_mask_size.x <= 0.0 || u_fog_mask_size.y <= 0.0) {
    discard;
  }

  vec2 wp = v_world_pos.xz;
  vec2 drift = vec2(u_time * 0.010, -u_time * 0.007);
  float large = vnoise(wp * 0.045 + drift);
  float detail = vnoise(wp * 0.120 + vec2(13.7, 4.1));
  float erosion = vnoise(wp * 0.080 + vec2(21.5, -7.4) + drift * 0.6);

  vec2 warp = (vec2(large, erosion) - vec2(0.5)) * u_fog_mask_tile_size * 0.85;
  vec2 uv = visibility_mask_uv(wp + warp, u_fog_mask_size, u_fog_mask_tile_size);
  float fog = texture(u_fog_mask_tex, uv).r;

  vec2 sight_uv = visibility_mask_uv(wp, u_fog_mask_size, u_fog_mask_tile_size);
  float seen_now = texture(u_fog_mask_tex, sight_uv).g;
  fog *= 1.0 - smoothstep(0.12, 0.55, seen_now);

  float body = smoothstep(0.16, 0.62, fog);
  if (body <= 0.002) {
    discard;
  }

  float alpha = v_alpha * body * mix(0.97, 1.0, large);

  if (alpha <= 0.004) {
    discard;
  }

  vec3 lit = v_color * mix(0.86, 1.14, detail);
  lit *= 0.94 + 0.10 * large;
  lit = mix(lit * 0.82, lit, smoothstep(0.30, 0.90, fog));

  frag_color = vec4(lit, clamp(alpha, 0.0, 1.0));
}
