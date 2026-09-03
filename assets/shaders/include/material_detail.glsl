

uniform sampler2D u_material_detail;
uniform bool u_has_material_detail;

const int k_material_mineral = 0;
const int k_material_metal = 1;
const int k_material_wood = 2;
const int k_material_cloth = 3;
const int k_material_leather = 4;

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

  vec3 cool_lime = base_color * vec3(0.94, 0.98, 1.025);
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

vec3 soi_wood_variation(vec3 base_color, vec2 uv, vec3 normal, float height) {
  float grain_field = soi_detail_mid(uv * 4.5);
  float grain = sin(height * 22.0 + grain_field * 3.5) * 0.5 + 0.5;
  float fine = soi_detail_micro(uv * 48.0) * 0.10;
  float knot = step(0.93, soi_detail_mid(uv * 2.2)) * 0.16;
  float wood_noise = grain * 0.13 + fine - knot;
  float view_angle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.3))));
  float sheen = pow(1.0 - view_angle, 4.0) * 0.06;
  return base_color * (1.0 + wood_noise) + vec3(sheen);
}

vec3 soi_metal_variation(vec3 base_color, vec2 uv, vec3 normal) {
  float metal_noise = soi_detail_fine(uv * 9.0) * 0.018;
  float view_angle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
  float fresnel = pow(1.0 - view_angle, 2.0) * 0.10;
  return base_color + vec3(metal_noise + fresnel);
}

vec3 soi_cloth_variation(vec3 base_color, vec2 uv, vec3 normal, vec3 world_pos) {
  float weave_pattern = sin(world_pos.x * 55.0) * sin(world_pos.z * 55.0) * 0.025;
  float cloth_noise = soi_detail_mid(uv * 2.5) * 0.10 - 0.05;
  float view_angle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
  float sheen = pow(1.0 - view_angle, 3.0) * 0.15;
  return base_color * (1.0 + cloth_noise + weave_pattern) + vec3(sheen);
}

vec3 soi_leather_variation(vec3 base_color, vec2 uv) {
  float leather_noise = soi_detail_fine(uv * 5.5);
  float blotches = soi_detail_coarse(uv * 1.8) * 0.12 - 0.06;
  return base_color * (1.0 + leather_noise * 0.14 - 0.07 + blotches);
}

vec3 soi_material_variation(vec3 base_color,
                            vec3 world_pos,
                            vec3 normal,
                            int material_id) {
  if (!u_has_material_detail) {
    return base_color;
  }
  vec2 uv = soi_surface_lattice(world_pos, normal) * 4.0;
  vec3 variation = base_color;
  if (material_id == k_material_mineral) {
    variation = soi_mineral_variation(base_color, uv, normal);
  } else if (material_id == k_material_wood) {
    variation = soi_wood_variation(base_color, uv, normal, world_pos.y);
  } else if (material_id == k_material_metal) {
    variation = soi_metal_variation(base_color, uv, normal);
  } else if (material_id == k_material_cloth) {
    variation = soi_cloth_variation(base_color, uv, normal, world_pos);
  } else if (material_id == k_material_leather) {
    variation = soi_leather_variation(base_color, uv);
  }
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
