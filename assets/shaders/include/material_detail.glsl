#include "ground_readability.glsl"
uniform sampler2D u_material_detail;
uniform bool u_has_material_detail;
uniform vec3 u_camera_pos;

const int k_material_mineral = 5;
const int k_material_metal = 1;
const int k_material_wood = 2;
const int k_material_cloth = 3;
const int k_material_leather = 4;
const int k_material_ceramic = 9;

const float k_detail_cells_coarse = 4.0;
const float k_detail_cells_mid = 16.0;
const float k_detail_cells_fine = 64.0;
const float k_detail_cells_micro = 128.0;

float soi_detail_coarse(vec2 lattice) {
  return texture(u_material_detail, lattice / k_detail_cells_coarse).r;
}

float soi_detail_mid(vec2 lattice) {
  return texture(u_material_detail, lattice / k_detail_cells_mid).g;
}

float soi_detail_fine(vec2 lattice) {
  return texture(u_material_detail, lattice / k_detail_cells_fine).b;
}

float soi_detail_micro(vec2 lattice) {
  return texture(u_material_detail, lattice / k_detail_cells_micro).a;
}

vec2 soi_surface_lattice(vec3 world_pos, vec3 normal) {
  vec3 axis = abs(normal);
  if (axis.x > axis.y && axis.x > axis.z) {
    return world_pos.zy;
  }
  if (axis.y > axis.z) {
    return world_pos.xz;
  }
  return world_pos.xy;
}

vec3 soi_mineral_variation(vec3 base_color, vec2 uv, vec3 normal) {
  float broad = soi_detail_coarse(uv * 0.70) - 0.5;
  float mottle = soi_detail_mid(uv * 2.3) - 0.5;
  float grain = soi_detail_fine(uv * 8.5) - 0.5;
  float upness = abs(normal.y);

  vec3 cool_lime = base_color * vec3(0.965, 0.985, 1.005);
  vec3 sun_worn = base_color * vec3(1.045, 0.995, 0.91);
  vec3 variation = mix(cool_lime, sun_worn, smoothstep(-0.35, 0.35, broad));
  variation *= 0.97 + broad * 0.10 + mottle * 0.075 + grain * 0.035;

  float vertical_streak =
      smoothstep(0.60, 0.90, soi_detail_mid(vec2(uv.x * 0.42, uv.y * 2.4)));
  vertical_streak *= (1.0 - upness) * (0.35 + 0.65 * soi_detail_coarse(uv * 0.28));
  variation =
      mix(variation, variation * vec3(0.72, 0.75, 0.71), vertical_streak * 0.12);

  float dust = upness * smoothstep(0.62, 0.92, soi_detail_coarse(uv * 0.46));
  variation = mix(variation, variation * vec3(1.04, 0.995, 0.90), dust * 0.08);
  return variation;
}

vec3 soi_wood_variation(
    vec3 base_color, vec2 uv, vec3 normal, vec3 view_dir, float height) {
  float grain_field = soi_detail_mid(uv * 4.5);
  float grain = sin(height * 22.0 + grain_field * 3.5) * 0.5 + 0.5;
  float fine = soi_detail_micro(uv * 48.0) * 0.10;
  float knot = step(0.93, soi_detail_mid(uv * 2.2)) * 0.16;
  float wood_noise = grain * 0.13 + fine - knot;
  float view_angle = abs(dot(normal, view_dir));
  float sheen = pow(1.0 - view_angle, 4.0) * 0.06;
  return base_color * (1.0 + wood_noise) + vec3(sheen);
}

const float k_grain_frame_offset = 4096.0;
const float k_grain_member_stride = 64.0;
const float k_board_width = 0.21;

bool soi_has_grain_frame(vec2 frame) {
  return frame.y > k_grain_frame_offset * 0.5;
}

float soi_wood_hash(float n) {
  return fract(sin(n * 91.3458 + 17.17) * 47453.5453);
}

float soi_line_fade(float width) {
  return 1.0 - smoothstep(0.25, 0.75, width);
}

vec3 soi_wood_grain(
    vec3 base_color, vec2 frame, vec3 normal, vec3 view_dir, float tactical) {
  float encoded = frame.y - k_grain_frame_offset;
  float timber = floor(encoded / k_grain_member_stride + 0.5);
  float along = encoded - timber * k_grain_member_stride;
  bool sawn = frame.x >= 0.0;
  bool round_side = frame.x < -0.5 && frame.x >= -2.5;
  bool end_grain = frame.x < -2.5 && frame.x >= -4.5;
  bool hewn = frame.x < -4.5;

  float across = frame.x;
  if (round_side) {
    across = (-1.0 - frame.x) * 0.75;
  } else if (end_grain) {
    across = (-3.0 - frame.x) * 0.12;
  } else if (hewn) {
    across = (-5.0 - frame.x) * 0.60;
  }

  float across_width = fwidth(across);
  float radius_width = fwidth(-3.0 - frame.x);

  float board = sawn ? floor(across / k_board_width) : 0.0;
  float seed = soi_wood_hash(timber * 7.13 + board * 3.71 + 0.5);
  float seed_b = soi_wood_hash(timber * 1.37 + board * 11.3 + 9.1);
  float phase = seed * 37.0;
  float micro = 1.0 - tactical * 0.6;

  float luma = dot(base_color, vec3(0.299, 0.587, 0.114));
  vec3 silver = vec3(luma) * vec3(1.00, 0.98, 0.94);
  vec3 warm = base_color * vec3(1.04, 0.99, 0.90);

  float chroma = max(base_color.r, max(base_color.g, base_color.b)) -
                 min(base_color.r, min(base_color.g, base_color.b));
  float weathering = (0.16 + 0.36 * seed_b + 0.22 * max(normal.y, 0.0)) *
                     (1.0 - smoothstep(0.25, 0.36, chroma));
  vec3 tone = mix(warm, silver, weathering) * (0.80 + 0.30 * seed);

  float streak = soi_detail_mid(vec2(across * 7.0 + phase, along * 0.45)) - 0.5;
  float fibre = soi_detail_fine(vec2(across * 34.0 + phase, along * 1.6)) - 0.5;
  vec3 color = tone * (1.0 + (streak * 0.28 + fibre * 0.14) * micro);

  float grain_field =
      soi_detail_fine(vec2(across * 46.0 + streak * 5.0 + phase, along * 0.7));
  float grain_line =
      smoothstep(0.60, 0.78, grain_field) * soi_line_fade(across_width * 46.0);
  color *= 1.0 - grain_line * 0.22 * micro;

  if (sawn) {
    float figure = sin((across + streak * 0.05) * 160.0 + phase);
    float latewood =
        smoothstep(0.55, 0.95, figure) * soi_line_fade(across_width * 26.0);
    color *= 1.0 - latewood * 0.10 * micro;
  }

  if (round_side) {
    float bark_field =
        soi_detail_coarse(vec2(across * 1.6 + phase, along * 0.30 + phase)) +
        clamp(-along * 0.18, -0.15, 0.20);
    float bark = smoothstep(0.52, 0.60, bark_field);
    float furrow = soi_detail_fine(vec2(across * 22.0 + phase, along * 0.9)) - 0.5;
    vec3 bark_color =
        base_color * vec3(0.50, 0.44, 0.40) * (1.0 + furrow * 0.55 * micro);
    color = mix(color, bark_color, bark * 0.85);
  }

  float check_line = abs(fract(across * 5.5 + streak * 0.6 + seed) - 0.5);
  float check_run =
      smoothstep(0.62, 0.72, soi_detail_mid(vec2(across * 3.0 + phase, along * 0.35)));
  float check = (1.0 - smoothstep(0.015, 0.04 + across_width * 6.0, check_line)) *
                check_run * soi_line_fade(across_width * 12.0);
  color *= 1.0 - check * 0.45 * micro;

  float knot_field = soi_detail_mid(vec2(across * 2.6 + phase, along * 1.1 + phase));
  float knot = smoothstep(0.88, 0.94, knot_field);
  color = mix(color, tone * vec3(0.46, 0.36, 0.28), knot * 0.70);

  if (sawn && across > k_board_width * 0.5) {
    float seam_distance =
        abs(fract(across / k_board_width + 0.5) - 0.5) * k_board_width;
    float seam_width = across_width;
    float seam = 1.0 - smoothstep(0.007, 0.007 + seam_width * 1.5, seam_distance);
    color *= 1.0 - seam * 0.55 * (1.0 - smoothstep(0.03, 0.09, seam_width));
  }

  if (end_grain) {
    float radius = -3.0 - frame.x;
    float ring_width = radius_width * 14.0;
    float rings = sin(radius * 44.0 + streak * 3.0) * soi_line_fade(ring_width);
    color = tone * vec3(1.16, 1.06, 0.92) * (1.0 + rings * 0.06 * micro);
    color *= 1.0 - smoothstep(0.82, 1.0, radius) * 0.25;
    color *= 1.0 - (1.0 - smoothstep(0.0, 0.10, radius)) * 0.30;
  }

  if (hewn) {
    float facet = soi_wood_hash(floor((-5.0 - frame.x) * 7.0) + timber * 5.0);
    color = tone * vec3(1.12, 1.04, 0.92) * (0.90 + facet * 0.18) *
            (1.0 + fibre * 0.10 * micro);
  }

  float view_angle = abs(dot(normal, view_dir));
  return color + vec3(pow(1.0 - view_angle, 4.0) * 0.035);
}

vec3 soi_metal_variation(vec3 base_color, vec2 uv, vec3 normal, vec3 view_dir) {
  float metal_noise = (soi_detail_fine(uv * 9.0) - 0.5) * 0.018;
  float view_angle = abs(dot(normal, view_dir));
  float fresnel = pow(1.0 - view_angle, 2.0) * 0.10;
  vec3 metal_tint = mix(base_color, sqrt(max(base_color, vec3(0.0))), 0.35);
  return base_color + metal_tint * (metal_noise + fresnel);
}

vec3 soi_cloth_variation(
    vec3 base_color, vec2 uv, vec3 normal, vec3 view_dir, vec3 world_pos) {
  float weave_visibility =
      1.0 -
      smoothstep(0.25, 0.75, max(fwidth(world_pos.x), fwidth(world_pos.z)) * 55.0);
  float weave_pattern =
      sin(world_pos.x * 55.0) * sin(world_pos.z * 55.0) * 0.018 * weave_visibility;
  float cloth_noise = soi_detail_mid(uv * 2.5) * 0.10 - 0.05;
  float view_angle = abs(dot(normal, view_dir));
  float sheen = pow(1.0 - view_angle, 3.0) * 0.045;
  return base_color * (1.0 + cloth_noise + weave_pattern + sheen);
}

vec3 soi_leather_variation(vec3 base_color, vec2 uv) {
  float leather_noise = soi_detail_fine(uv * 5.5);
  float blotches = soi_detail_coarse(uv * 1.8) * 0.12 - 0.06;
  return base_color * (1.0 + leather_noise * 0.14 - 0.07 + blotches);
}

vec3 soi_ceramic_variation(
    vec3 base_color, vec2 uv, vec3 normal, vec3 view_dir, vec3 world_pos) {
  float firing = soi_detail_coarse(uv * 0.9) - 0.5;
  float mottle = soi_detail_mid(uv * 3.1) - 0.5;
  float grit = soi_detail_fine(uv * 11.0) - 0.5;
  vec3 kiln_warm = base_color * vec3(1.08, 0.97, 0.86);
  vec3 kiln_smoke = base_color * vec3(0.76, 0.73, 0.71);
  vec3 variation = mix(kiln_warm, kiln_smoke, smoothstep(0.08, 0.42, firing));
  variation *= 0.97 + mottle * 0.09 + grit * 0.05;

  float side = 1.0 - abs(normal.y);
  float ring_visibility = 1.0 - smoothstep(0.2, 0.8, fwidth(world_pos.y) * 90.0);
  float throwing_rings = sin(world_pos.y * 260.0 + mottle * 2.0);
  variation *= 1.0 + throwing_rings * 0.028 * ring_visibility * side;

  float upness = max(normal.y, 0.0);
  float dust = upness * smoothstep(0.45, 0.85, soi_detail_coarse(uv * 0.5));
  variation = mix(variation, vec3(0.70, 0.62, 0.50), dust * 0.20);

  float chip = step(0.955, soi_detail_mid(uv * 5.3)) * side;
  variation = mix(variation, base_color * vec3(1.18, 1.05, 0.92), chip * 0.35);

  float view_angle = abs(dot(normal, view_dir));
  float burnish = pow(1.0 - view_angle, 3.0) * 0.06;
  return variation + base_color * burnish;
}

vec3 soi_material_variation(
    vec3 base_color, vec3 world_pos, vec3 normal, int material_id, vec2 grain_frame) {
  float tactical = ground_tactical_distance(length(u_camera_pos - world_pos));

  if (material_id == k_material_mineral || material_id == 0 ||
      material_id == k_material_wood || material_id == k_material_ceramic) {
    float luma = dot(base_color, vec3(0.299, 0.587, 0.114));
    float saturation = material_id == k_material_wood ? 0.94 : 0.93;
    base_color = mix(vec3(luma), base_color, mix(1.0, saturation, tactical));
  }
  if (!u_has_material_detail) {
    return base_color;
  }
  vec3 view_dir = normalize(u_camera_pos - world_pos);

  bool timber_colour = base_color.g > base_color.r * 0.66;
  if (material_id == k_material_wood && timber_colour &&
      soi_has_grain_frame(grain_frame)) {
    return clamp(
        soi_wood_grain(base_color, grain_frame, normal, view_dir, tactical), 0.0, 1.0);
  }
  vec2 uv = soi_surface_lattice(world_pos, normal) * 4.0;
  vec3 variation = base_color;
  if (material_id == k_material_mineral || material_id == 0) {
    variation = soi_mineral_variation(base_color, uv, normal);
  } else if (material_id == k_material_wood) {
    variation = soi_wood_variation(base_color, uv, normal, view_dir, world_pos.y);
  } else if (material_id == k_material_metal) {
    variation = soi_metal_variation(base_color, uv, normal, view_dir);
  } else if (material_id == k_material_cloth) {
    variation = soi_cloth_variation(base_color, uv, normal, view_dir, world_pos);
  } else if (material_id == k_material_leather) {
    variation = soi_leather_variation(base_color, uv);
  } else if (material_id == k_material_ceramic) {
    variation = soi_ceramic_variation(base_color, uv, normal, view_dir, world_pos);
  }

  variation = mix(base_color, variation, mix(1.0, 0.55, tactical));
  return clamp(variation, 0.0, 1.0);
}

vec3 soi_apply_damage_soot(vec3 color, vec3 world_pos, int damage_tier) {
  if (damage_tier <= 0 || !u_has_material_detail) {
    return color;
  }
  float soot_amt = float(damage_tier) * 0.45;
  vec2 soot_uv = world_pos.xz * 3.5;
  float soot_patch =
      soi_detail_coarse(soot_uv) * 0.6 + soi_detail_mid(soot_uv * 4.1) * 0.4;
  float soot_mask = smoothstep(0.42, 0.65, soot_patch) * soot_amt;
  vec3 char_color = mix(color * 0.25, vec3(0.08, 0.07, 0.06), 0.5);
  return mix(color, char_color, clamp(soot_mask, 0.0, 0.85));
}
