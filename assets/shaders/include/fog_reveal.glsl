uniform sampler2D u_fog_mask_tex;
uniform vec2 u_fog_mask_size;
uniform float u_fog_mask_tile_size;
uniform int u_has_fog_mask;

const float k_fog_reveal_cutoff = 0.02;

bool fog_reveal_active() {
  return u_has_fog_mask == 1 && u_fog_mask_size.x > 0.0 && u_fog_mask_size.y > 0.0;
}

vec2 fog_reveal_uv(vec2 world_xz, vec2 mask_size, float mask_tile_size) {
  float tile = max(mask_tile_size, 0.0001);
  vec2 grid = world_xz / tile;
  grid += (mask_size * 0.5) - vec2(0.5);
  return (grid + vec2(0.5)) / mask_size;
}

vec2 fog_reveal_sample(vec2 world_xz) {
  if (!fog_reveal_active()) {
    return vec2(1.0, 1.0);
  }
  vec2 uv = fog_reveal_uv(world_xz, u_fog_mask_size, u_fog_mask_tile_size);
  vec2 texel = texture(u_fog_mask_tex, uv).rg;
  return vec2(clamp(1.0 - texel.r, 0.0, 1.0), clamp(texel.g, 0.0, 1.0));
}

float fog_reveal_alpha(float reveal) {
  return smoothstep(0.04, 0.94, reveal);
}

bool fog_reveal_discards(float alpha) {
  return alpha <= k_fog_reveal_cutoff;
}

vec3 fog_reveal_haze(vec3 lit_color, float alpha) {
  float lum = dot(lit_color, vec3(0.299, 0.587, 0.114));
  vec3 misted = mix(vec3(lum), lit_color, 0.30) * 0.66;
  return mix(misted, lit_color, alpha);
}
