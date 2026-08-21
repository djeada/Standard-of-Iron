#include "noise.glsl"

#ifndef SOI_CHARACTER_VARIANT
#define SOI_CHARACTER_VARIANT 0
#endif

#define SOI_CHARACTER_ANY 0
#define SOI_CHARACTER_HUMANOID 1
#define SOI_CHARACTER_HORSE 2
#define SOI_CHARACTER_WILDLIFE 3
#define SOI_CHARACTER_ELEPHANT 4

#define SOI_CHARACTER_WANTS(variant)                                                   \
  (SOI_CHARACTER_VARIANT == SOI_CHARACTER_ANY || SOI_CHARACTER_VARIANT == (variant))

uniform sampler3D u_wear_volume;
uniform bool u_has_wear_volume;

const float k_wear_volume_cells = 32.0;

const int k_wildlife_material = 7;
const float k_wildlife_belly_y = 0.26;
const float k_wildlife_back_span = 0.40;
const float k_wildlife_rim = 0.030;
const float k_wildlife_shadow_floor = 0.42;
const int k_elephant_material = 8;
const int k_elephant_role_tusk = 6;
const int k_elephant_role_eye = 7;

const float k_elephant_height_local = 1.75;
const float k_elephant_rim_cancel = 0.78;
const float k_elephant_shadow_floor = 0.26;
const float k_elephant_sun_rim_scale = 0.30;
const float k_elephant_diffuse_wrap = 0.10;
const float k_elephant_belly_shade = 0.22;
const float k_elephant_highlight_knee = 0.72;
const float k_elephant_hide_saturation = 0.70;

vec4 soi_wear_bands(vec3 lattice) {
  return texture(u_wear_volume, lattice / k_wear_volume_cells);
}

float soi_wear_hash(vec3 lattice, float salt) {
  return soi_hash13_a1b3c9(floor(lattice) + salt);
}

vec4 soi_wear_fetch(vec3 lattice, vec4 salts) {
  if (u_has_wear_volume) {
    return soi_wear_bands(lattice);
  }
  return vec4(soi_wear_hash(lattice, salts.x),
              soi_wear_hash(lattice, salts.y),
              soi_wear_hash(lattice, salts.z),
              soi_wear_hash(lattice, salts.w));
}

#if SOI_CHARACTER_WANTS(SOI_CHARACTER_WILDLIFE)
vec3 apply_wildlife_coat(vec3 base, vec3 pos_local) {
  vec3 coat_pos = abs(pos_local);
  vec4 bands =
      soi_wear_fetch(coat_pos * vec3(7.0, 11.0, 9.0), vec4(3.0, 11.0, 5.0, 7.0));
  float macro = bands.x;
  float micro =
      soi_wear_fetch(coat_pos.zxy * vec3(23.0, 19.0, 29.0), vec4(11.0, 3.0, 5.0, 7.0))
          .y;
  float back =
      clamp((pos_local.y - k_wildlife_belly_y) / k_wildlife_back_span, 0.0, 1.0);
  vec3 coat = base * (0.94 + macro * 0.09);
  coat = mix(coat * vec3(0.68, 0.64, 0.60), coat, back);
  coat = mix(coat, min(coat * 1.10, vec3(1.0)), back * back * 0.55 + micro * 0.06);
  return clamp(coat, 0.0, 1.0);
}

#endif

#if SOI_CHARACTER_WANTS(SOI_CHARACTER_ELEPHANT)
vec3 apply_elephant_hide(vec3 base, int color_role, vec3 pos_local) {
  if (color_role == k_elephant_role_eye) {
    return base;
  }
  float height = clamp(pos_local.y / k_elephant_height_local, 0.0, 1.0);

  if (color_role == k_elephant_role_tusk) {
    float grain =
        soi_wear_fetch(pos_local * vec3(37.0, 41.0, 39.0), vec4(71.0, 5.0, 19.0, 29.0))
            .x;
    float root = smoothstep(0.42, 0.68, height);
    vec3 ivory = base * (0.95 + grain * 0.07);
    ivory = mix(ivory, ivory * vec3(0.80, 0.72, 0.58), root * 0.45);
    return clamp(ivory, 0.0, 1.0);
  }

  vec4 coarse_bands = soi_wear_fetch(pos_local * 19.0, vec4(5.0, 29.0, 19.0, 43.0));
  float coarse = coarse_bands.x;
  float blotch_field = soi_wear_fetch(pos_local * 4.0, vec4(29.0, 5.0, 19.0, 43.0)).y;
  float fine = soi_wear_fetch(pos_local * 43.0, vec4(19.0, 43.0, 5.0, 29.0)).z;
  float fold =
      0.5 + 0.5 * sin(pos_local.y * 46.0 + pos_local.z * 6.0 + coarse * 6.2831);
  float crease =
      0.5 + 0.5 * sin(pos_local.z * 31.0 + pos_local.x * 23.0 + fine * 6.2831);
  float wrinkle = clamp(fold * 0.5 + crease * 0.5, 0.0, 1.0);
  vec3 hide = base * (0.86 + 0.15 * wrinkle + 0.06 * (coarse - 0.5));

  float blotch = smoothstep(0.20, 0.85, blotch_field);
  float back = smoothstep(0.62, 1.0, height);
  float legs = 1.0 - smoothstep(0.04, 0.34, height);
  float dust = clamp((back * 0.70 + legs * 0.85) * (0.40 + 0.60 * blotch), 0.0, 1.0);
  hide = mix(hide, hide * vec3(1.10, 1.03, 0.90), dust * 0.45);
  hide *= 1.0 - (1.0 - smoothstep(0.24, 0.62, height)) * k_elephant_belly_shade;
  return clamp(hide, 0.0, 1.0);
}

#endif

vec3 apply_wear(vec3 base, int material_id, int color_role, vec3 pos_local, vec4 wear) {
#if SOI_CHARACTER_WANTS(SOI_CHARACTER_WILDLIFE)
  if (material_id == k_wildlife_material) {
    return apply_wildlife_coat(base, pos_local);
  }
#endif
#if SOI_CHARACTER_WANTS(SOI_CHARACTER_ELEPHANT)
  if (material_id == k_elephant_material) {
    return apply_elephant_hide(base, color_role, pos_local);
  }
#endif
#if SOI_CHARACTER_WANTS(SOI_CHARACTER_HORSE)
  bool horse_material = material_id == 6;
  bool horse_hoof = color_role == 4;
  bool horse_hair = color_role == 5 || color_role == 6;
  bool horse_muzzle = color_role == 7;
  bool horse_eye = color_role == 8;
  if (horse_eye) {
    return base;
  }
  if (horse_hoof) {
    vec3 hoof_pos = abs(pos_local) * vec3(11.0, 6.0, 13.0);
    float horn = soi_wear_fetch(hoof_pos, vec4(5.0, 7.0, 13.0, 31.0)).x;
    float band = 0.5 + 0.5 * sin(pos_local.y * 24.0 + pos_local.z * 7.0);
    vec3 hoof = base * (0.92 + horn * 0.06);
    hoof += vec3(0.01) * band * 0.14;
    return clamp(hoof, 0.0, 1.0);
  }
  if (horse_material) {
    vec3 coat_pos = abs(pos_local);
    float macro =
        soi_wear_fetch(coat_pos * vec3(5.0, 9.0, 7.0), vec4(7.0, 13.0, 31.0, 5.0)).x;
    vec4 fine_bands = soi_wear_fetch(coat_pos.yzx * vec3(21.0, 17.0, 23.0),
                                     vec4(13.0, 31.0, 7.0, 5.0));
    float micro = fine_bands.y;
    float stray = fine_bands.z;
    float streak =
        0.5 + 0.5 * sin(pos_local.z * 34.0 + pos_local.y * 18.0 + macro * 6.2831);
    float dorsal = clamp(pos_local.y * 0.9 + 0.55, 0.0, 1.0);
    vec3 fur = base;
    if (horse_hair) {
      float strand = 0.70 + 0.30 * streak;
      fur *= 0.90 + strand * 0.16;
      fur = mix(fur, fur * vec3(0.80, 0.74, 0.68), stray * 0.10);
      fur = mix(fur, min(fur * 1.10, vec3(1.0)), dorsal * 0.10);
    } else if (horse_muzzle) {
      float soft = 0.88 + macro * 0.10;
      fur *= soft;
      fur = mix(fur, fur * vec3(0.72, 0.66, 0.62), (1.0 - dorsal) * 0.18);
    } else {
      float underside = clamp((-pos_local.y + 0.10) * 0.8, 0.0, 1.0);
      fur *= 0.92 + macro * 0.10 - underside * 0.06;
      fur = mix(fur, min(fur * 1.08, vec3(1.0)), dorsal * 0.12 + micro * 0.05);
      fur = mix(fur, fur * vec3(0.82, 0.80, 0.78), streak * 0.04);
    }
    return clamp(fur, 0.0, 1.0);
  }
#endif

#if SOI_CHARACTER_WANTS(SOI_CHARACTER_HUMANOID)
  float wear_amount = clamp(wear.x * 1.35, 0.0, 1.0);
  float grime_amount = clamp(wear.y * 1.35, 0.0, 1.0);
  float blood_amount = clamp(wear.z * 1.75, 0.0, 1.0);
  float seed = wear.w + float(material_id) * 0.173;

  vec3 abs_pos = abs(pos_local);
  vec3 mask_p = abs_pos * (2.8 + float(material_id & 3)) + vec3(seed * 13.0);

  vec4 coarse_bands = soi_wear_fetch(mask_p * 2.0, vec4(3.0, 41.0, 17.0, 59.0));
  vec4 blotch_bands = soi_wear_fetch(mask_p * 4.5, vec4(0.0, 29.0, 23.0, 59.0));
  float macro = coarse_bands.x;
  float shade_variation = 0.78 + 0.42 * coarse_bands.y;
  float micro = soi_wear_fetch(mask_p * 9.0, vec4(17.0, 3.0, 41.0, 59.0)).x;
  float blotch = blotch_bands.x;
  float streak = 0.5 + 0.5 * sin(mask_p.y * 8.0 + seed * 19.0 + mask_p.z * 6.0);
  float edge_mask =
      smoothstep(0.18, 0.95, max(abs_pos.x, max(abs_pos.y * 0.75, abs_pos.z)));
  float grime_mask =
      smoothstep(0.18, 0.88, macro * 0.45 + blotch * 0.30 + streak * 0.25);
  float wear_mask =
      max(edge_mask * 0.75, smoothstep(0.34, 0.86, blotch * 0.55 + micro * 0.45));
  float fade_mask = smoothstep(0.28, 0.82, blotch_bands.y);
  float blood_mask = smoothstep(0.66, 0.94, blotch_bands.z);

  float max_component = max(base.r, max(base.g, base.b));
  float min_component = min(base.r, min(base.g, base.b));
  float saturation = max_component - min_component;
  float brightness = (base.r + base.g + base.b) / 3.0;
  float metal_like = max(float(material_id == 2 || material_id == 6),
                         smoothstep(0.18, 0.58, max_component) *
                             (1.0 - smoothstep(0.08, 0.30, saturation)));
  float leather_like =
      max(float(material_id == 1 || material_id == 3 || material_id == 4),
          (1.0 - metal_like) * smoothstep(0.08, 0.32, saturation) *
              smoothstep(0.18, 0.72, base.r));
  float cloth_like = clamp(1.0 - metal_like * 0.8, 0.0, 1.0);
  if (color_role == 2) {
    wear_amount *= 0.45;
    grime_amount *= 0.18;
    blood_amount *= 0.08;
  }

  vec3 worn = base;
  vec3 grayscale = vec3(brightness);
  worn *= mix(1.0, shade_variation, 0.20 * cloth_like + 0.12 * leather_like);
  worn = mix(worn,
             worn * vec3(0.62, 0.58, 0.52),
             wear_amount * fade_mask * (0.40 * cloth_like + 0.14 * leather_like));
  worn = mix(worn,
             grayscale * vec3(0.96, 0.92, 0.84),
             wear_amount * wear_mask * (0.14 + 0.32 * metal_like));
  worn *= 1.0 -
          grime_amount * grime_mask * (0.24 + 0.18 * cloth_like + 0.14 * leather_like);
  worn = mix(worn,
             worn * vec3(0.55, 0.72, 0.52),
             wear_amount * grime_mask * metal_like * 0.34);
  worn = mix(worn,
             worn * vec3(0.72, 0.56, 0.44),
             grime_amount * grime_mask * leather_like * 0.26);
  worn = mix(worn,
             min(worn * 1.22, vec3(1.0)),
             wear_amount * micro * edge_mask * metal_like * 0.14);
  if (color_role == 2) {
    float bruise_mask = smoothstep(0.74, 0.96, blotch_bands.w);
    worn = mix(worn, worn * vec3(0.82, 0.68, 0.68), wear_amount * bruise_mask * 0.22);
  }
  worn =
      mix(worn,
          vec3(0.42, 0.09, 0.07),
          blood_amount * blood_mask * (0.18 + 0.60 * cloth_like + 0.24 * leather_like));
  return clamp(worn, 0.0, 1.0);
#else
  return base;
#endif
}
