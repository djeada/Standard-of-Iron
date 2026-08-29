

uniform sampler2D u_visibility_tex;
uniform vec2 u_visibility_size;
uniform float u_visibility_tile_size;
uniform float u_explored_alpha;
uniform int u_has_visibility;

const float k_visibility_known_cutoff = 0.30;
const float k_visibility_unseen_cutoff = 0.06;

const float k_visibility_unseen_blend_end = 0.52;

const vec3 k_visibility_unseen_shade = vec3(0.22, 0.23, 0.25);
const vec3 k_visibility_unseen_lift = vec3(0.030, 0.036, 0.048);
const float k_visibility_unseen_chroma = 0.70;

struct VisibilityMask {
  float seen_now;
  float known;
};

vec2 visibility_mask_uv(vec2 world_xz, vec2 mask_size, float mask_tile_size) {
  float tile = max(mask_tile_size, 0.0001);
  vec2 grid = world_xz / tile;
  grid += (mask_size * 0.5) - vec2(0.5);
  return (grid + vec2(0.5)) / mask_size;
}

VisibilityMask sample_visibility_mask(sampler2D mask_tex,
                                      vec2 world_xz,
                                      vec2 mask_size,
                                      float mask_tile_size) {
  vec2 texel =
      texture(mask_tex, visibility_mask_uv(world_xz, mask_size, mask_tile_size)).rg;
  VisibilityMask mask;
  mask.seen_now = texel.r;
  mask.known = texel.g;
  return mask;
}

bool visibility_is_unknown(VisibilityMask mask) {
  return mask.known < k_visibility_known_cutoff;
}

bool visibility_is_unseen(VisibilityMask mask) {
  return mask.known < k_visibility_unseen_cutoff;
}

float visibility_unseen_blend(VisibilityMask mask) {
  return smoothstep(
      k_visibility_unseen_cutoff, k_visibility_unseen_blend_end, mask.known);
}

vec3 unseen_surface_color(vec3 lit_color) {
  float luminance = dot(lit_color, vec3(0.2126, 0.7152, 0.0722));
  vec3 shaded = mix(vec3(luminance), lit_color, k_visibility_unseen_chroma);

  return shaded * k_visibility_unseen_shade + k_visibility_unseen_lift;
}

float visibility_known_weight(VisibilityMask mask) {
  return smoothstep(k_visibility_unseen_cutoff, k_visibility_known_cutoff, mask.known);
}

float visibility_live_weight(VisibilityMask mask) {
  return smoothstep(0.18, 0.86, mask.seen_now);
}

float visibility_memory_falloff(VisibilityMask mask) {
  return mix(0.72, 1.0, smoothstep(k_visibility_known_cutoff, 0.92, mask.known));
}

vec3 remembered_surface_color(vec3 lit_color, float explored_alpha) {
  float luminance = dot(lit_color, vec3(0.2126, 0.7152, 0.0722));

  vec3 memory = mix(vec3(luminance), lit_color, 0.82);
  return memory * vec3(0.94, 0.97, 1.02) * explored_alpha;
}

bool visibility_mask_active() {
  return u_has_visibility == 1 && u_visibility_size.x > 0.0 &&
         u_visibility_size.y > 0.0;
}

VisibilityMask visibility_mask_fetch(vec2 world_xz) {
  if (!visibility_mask_active()) {
    VisibilityMask mask;
    mask.seen_now = 1.0;
    mask.known = 1.0;
    return mask;
  }
  return sample_visibility_mask(
      u_visibility_tex, world_xz, u_visibility_size, u_visibility_tile_size);
}

vec3 apply_visibility_memory_mask(vec3 lit_color, VisibilityMask mask) {
  if (!visibility_mask_active()) {
    return lit_color;
  }
  vec3 memory = remembered_surface_color(lit_color, u_explored_alpha) *
                visibility_memory_falloff(mask);
  vec3 charted = mix(memory, lit_color, visibility_live_weight(mask));
  return mix(unseen_surface_color(lit_color), charted, visibility_known_weight(mask));
}

vec3 apply_visibility_world_shading(vec3 lit_color, VisibilityMask mask) {
  if (!visibility_mask_active()) {
    return lit_color;
  }
  vec3 memory = remembered_surface_color(lit_color, u_explored_alpha) *
                visibility_memory_falloff(mask);
  vec3 shaded = mix(memory, lit_color, visibility_live_weight(mask));

  return mix(unseen_surface_color(lit_color), shaded, visibility_unseen_blend(mask));
}

vec3 apply_visibility_world_shading(vec3 lit_color, vec2 world_xz) {
  if (!visibility_mask_active()) {
    return lit_color;
  }
  return apply_visibility_world_shading(lit_color, visibility_mask_fetch(world_xz));
}

vec3 apply_visibility_memory(vec3 lit_color, vec2 world_xz) {
  if (!visibility_mask_active()) {
    return lit_color;
  }
  VisibilityMask mask = sample_visibility_mask(
      u_visibility_tex, world_xz, u_visibility_size, u_visibility_tile_size);
  if (visibility_is_unknown(mask)) {
    discard;
  }
  vec3 memory = remembered_surface_color(lit_color, u_explored_alpha) *
                visibility_memory_falloff(mask);
  return mix(memory, lit_color, visibility_live_weight(mask));
}
