#version 330 core
#include "noise.glsl"

in vec2 v_uv;
in vec2 v_world_pos;

uniform vec4 u_color;

uniform sampler2D u_base_texture;
uniform bool u_use_parchment;
uniform float u_parchment_scale;
uniform float u_parchment_strength;

out vec4 frag_color;

float get_parchment_mask(vec2 uv) {

  float n1 = soi_fbm_d6cc9d(uv * u_parchment_scale, 4);
  float n2 = soi_fbm_d6cc9d(uv * u_parchment_scale * 2.5 + vec2(100.0), 3);
  float n3 = soi_fbm_d6cc9d(uv * u_parchment_scale * 5.0 + vec2(200.0), 2);

  float combined = n1 * 0.5 + n2 * 0.35 + n3 * 0.15;

  return 0.85 + combined * 0.15;
}

void main() {

  vec2 base_uv = vec2(v_uv.x, 1.0 - v_uv.y);
  vec4 base_color = textureLod(u_base_texture, base_uv, 0.0);
  float land = smoothstep(-0.01, 0.05, base_color.r - base_color.b);
  if (land <= 0.001) {
    discard;
  }

  vec4 color = u_color;
  color.a *= land;

  if (u_use_parchment) {
    float parchment = get_parchment_mask(v_uv);
    color.rgb *= mix(1.0, parchment, u_parchment_strength);
  }

  frag_color = color;
}
