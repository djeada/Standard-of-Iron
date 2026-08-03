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

// Ink laid on paper varies; a flat wash reads as a decal. Three octaves at
// increasing frequency give the blotchiness of a hand-tinted political map.
float get_parchment_mask(vec2 uv) {

  float n1 = soi_fbm_d6cc9d(uv * u_parchment_scale, 4);
  float n2 = soi_fbm_d6cc9d(uv * u_parchment_scale * 2.5 + vec2(100.0), 3);
  float n3 = soi_fbm_d6cc9d(uv * u_parchment_scale * 5.0 + vec2(200.0), 2);

  float combined = n1 * 0.5 + n2 * 0.35 + n3 * 0.15;

  return 0.85 + combined * 0.15;
}

void main() {
  // The same land mask the terrain pass uses, read from the same texture: cream
  // paper is warm, open sea is cold.
  //
  // The pipeline already clips province polygons against the land mesh, so this
  // is a second, cheaper opinion rather than the primary defence -- it catches
  // the one to two percent of a seaboard province that lands on water because
  // the land mesh and the drawn coastline are generalised from the same data to
  // different tolerances. Estuaries, the Venetian lagoon and the straits either
  // side of Gibraltar are where it shows.
  //
  // Sampled at level 0 explicitly. These are a handful of very large triangles,
  // and letting the hardware pick a mip collapses the lookup to a single
  // averaged texel -- every province then reads as solid land and the mask does
  // nothing. A coastline mask has to come off the full-resolution image anyway.
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
