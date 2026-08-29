uniform sampler2D u_fog_mask_tex;
uniform vec2 u_fog_mask_size;
uniform float u_fog_mask_tile_size;
uniform int u_has_fog_mask;

const vec3 k_fog_reveal_shade = vec3(0.22, 0.23, 0.25);
const vec3 k_fog_reveal_lift = vec3(0.030, 0.036, 0.048);
const float k_fog_reveal_chroma = 0.70;

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

vec3 fog_reveal_haze(vec3 lit_color, float alpha) {
  float lum = dot(lit_color, vec3(0.2126, 0.7152, 0.0722));
  vec3 misted = mix(vec3(lum), lit_color, k_fog_reveal_chroma) * k_fog_reveal_shade +
                k_fog_reveal_lift;
  return mix(misted, lit_color, alpha);
}
