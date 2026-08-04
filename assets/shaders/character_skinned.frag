#version 330 core
#include "directional_shadows.glsl"
#include "environment_lighting.glsl"
#include "local_lighting.glsl"
#include "noise.glsl"

in vec3 v_normal_ws;
in vec2 v_tex;
in vec3 v_pos_ws;
in vec3 v_pos_local;
flat in int v_color_role;
flat in vec4 v_wear_params;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_use_texture;
uniform float u_alpha;
uniform int u_material_id;
uniform vec3 u_role_colors[32];
uniform int u_role_color_count;
uniform vec3 u_camera_position;

out vec4 frag_color;

const float k_readable_zoom_near = 18.0;
const float k_readable_zoom_far = 70.0;
const float k_readable_pivot = 0.42;
const float k_readable_contrast_gain = 0.38;
const float k_readable_saturation_gain = 0.30;
const float k_readable_fill_near = 0.18;
const float k_readable_fill_far = 0.22;
const float k_readable_rim_near = 0.040;
const float k_readable_rim_far = 0.055;
const float k_readable_wear_far = 0.30;
const float k_readable_grime_far = 0.20;
const float k_readable_blood_far = 0.75;
const float k_readable_shadow_floor = 0.60;
const int k_wildlife_material = 7;
const float k_wildlife_belly_y = 0.26;
const float k_wildlife_back_span = 0.40;
const float k_wildlife_rim = 0.030;

float readable_zoom(vec3 world_position) {
  float view_distance = length(u_camera_position - world_position);
  return smoothstep(k_readable_zoom_near, k_readable_zoom_far, view_distance);
}

vec3 apply_zoom_readability(vec3 color, float zoom) {
  if (zoom <= 0.0) {
    return color;
  }

  vec3 contrasted =
      (color - k_readable_pivot) * (1.0 + k_readable_contrast_gain * zoom) +
      k_readable_pivot;
  contrasted = clamp(contrasted, 0.0, 1.0);
  float luma = dot(contrasted, vec3(0.299, 0.587, 0.114));
  vec3 saturated = mix(vec3(luma), contrasted, 1.0 + k_readable_saturation_gain * zoom);
  return clamp(saturated, 0.0, 1.0);
}

vec3 apply_wildlife_coat(vec3 base, vec3 pos_local) {
  vec3 coat_pos = abs(pos_local);
  float macro = soi_hash13_a1b3c9(floor(coat_pos * vec3(7.0, 11.0, 9.0)) + 3.0);
  float micro = soi_hash13_a1b3c9(floor(coat_pos.zxy * vec3(23.0, 19.0, 29.0)) + 11.0);
  float back =
      clamp((pos_local.y - k_wildlife_belly_y) / k_wildlife_back_span, 0.0, 1.0);
  vec3 coat = base * (0.94 + macro * 0.09);
  coat = mix(coat * vec3(0.68, 0.64, 0.60), coat, back);
  coat = mix(coat, min(coat * 1.10, vec3(1.0)), back * back * 0.55 + micro * 0.06);
  return clamp(coat, 0.0, 1.0);
}

vec3 apply_wear(vec3 base, int material_id, int color_role, vec3 pos_local, vec4 wear) {
  if (material_id == k_wildlife_material) {
    return apply_wildlife_coat(base, pos_local);
  }
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
    float horn = soi_hash13_a1b3c9(floor(hoof_pos) + 5.0);
    float band = 0.5 + 0.5 * sin(pos_local.y * 24.0 + pos_local.z * 7.0);
    vec3 hoof = base * (0.92 + horn * 0.06);
    hoof += vec3(0.01) * band * 0.14;
    return clamp(hoof, 0.0, 1.0);
  }
  if (horse_material) {
    vec3 coat_pos = abs(pos_local);
    float macro = soi_hash13_a1b3c9(floor(coat_pos * vec3(5.0, 9.0, 7.0)) + 7.0);
    float micro =
        soi_hash13_a1b3c9(floor(coat_pos.yzx * vec3(21.0, 17.0, 23.0)) + 13.0);
    float streak =
        0.5 + 0.5 * sin(pos_local.z * 34.0 + pos_local.y * 18.0 + macro * 6.2831);
    float dorsal = clamp(pos_local.y * 0.9 + 0.55, 0.0, 1.0);
    vec3 fur = base;
    if (horse_hair) {
      float strand = 0.70 + 0.30 * streak;
      float stray =
          soi_hash13_a1b3c9(floor(coat_pos.zxy * vec3(31.0, 19.0, 27.0)) + 31.0);
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

  float wear_amount = clamp(wear.x * 1.35, 0.0, 1.0);
  float grime_amount = clamp(wear.y * 1.35, 0.0, 1.0);
  float blood_amount = clamp(wear.z * 1.75, 0.0, 1.0);
  float seed = wear.w + float(material_id) * 0.173;

  vec3 abs_pos = abs(pos_local);
  vec3 mask_p = abs_pos * (2.8 + float(material_id & 3)) + vec3(seed * 13.0);
  float macro = soi_hash13_a1b3c9(floor(mask_p * 2.0) + 3.0);
  float blotch = soi_hash13_a1b3c9(floor(mask_p * 3.0));
  float micro = soi_hash13_a1b3c9(floor(mask_p * 9.0) + 17.0);
  float streak = 0.5 + 0.5 * sin(mask_p.y * 8.0 + seed * 19.0 + mask_p.z * 6.0);
  float edge_mask =
      smoothstep(0.18, 0.95, max(abs_pos.x, max(abs_pos.y * 0.75, abs_pos.z)));
  float grime_mask =
      smoothstep(0.18, 0.88, macro * 0.45 + blotch * 0.30 + streak * 0.25);
  float wear_mask =
      max(edge_mask * 0.75, smoothstep(0.34, 0.86, blotch * 0.55 + micro * 0.45));
  float fade_mask =
      smoothstep(0.28, 0.82, soi_hash13_a1b3c9(floor(mask_p.yzx * 4.0) + 29.0));
  float blood_mask =
      smoothstep(0.66, 0.94, soi_hash13_a1b3c9(floor(mask_p.yzx * 5.0) + 23.0));
  float shade_variation =
      0.78 + 0.42 * soi_hash13_a1b3c9(floor(mask_p.zxy * 3.0) + 41.0);

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
    float bruise_mask =
        smoothstep(0.74, 0.96, soi_hash13_a1b3c9(floor(mask_p * 6.0) + 59.0));
    worn = mix(worn, worn * vec3(0.82, 0.68, 0.68), wear_amount * bruise_mask * 0.22);
  }
  worn =
      mix(worn,
          vec3(0.42, 0.09, 0.07),
          blood_amount * blood_mask * (0.18 + 0.60 * cloth_like + 0.24 * leather_like));
  return clamp(worn, 0.0, 1.0);
}

vec3 shade_readable_character(
    vec3 base, vec3 surface_normal, vec3 world_position, int material_id, float zoom) {
  vec3 light_dir = environment_primary_direction();
  vec3 view_dir = normalize(u_camera_position - world_position);
  float scene_ambient = clamp(environment_ambient_intensity(), 0.08, 0.40);
  float readable_ambient = max(scene_ambient, 0.22);
  vec3 sun_color = environment_primary_color();
  vec3 sky_color = environment_sky_color();

  float ndl = dot(surface_normal, light_dir);
  float wrapped_diffuse = clamp((ndl + 0.25) / 1.25, 0.0, 1.0);
  float hemisphere = clamp(surface_normal.y * 0.5 + 0.5, 0.0, 1.0);
  float fill = readable_ambient * (0.78 + hemisphere * 0.22);
  float direct = wrapped_diffuse * environment_primary_intensity();
  vec3 ambient_light =
      mix(environment_ground_bounce_color(), sky_color, hemisphere) * readable_ambient;
  vec3 color = base * (ambient_light + sun_color * direct) * environment_exposure();
  color += base * local_lighting(world_position, surface_normal);

  float shadow_side = 1.0 - wrapped_diffuse;
  color += base * sky_color * fill * shadow_side *
           mix(k_readable_fill_near, k_readable_fill_far, zoom);

  float rim = pow(1.0 - max(dot(surface_normal, view_dir), 0.0), 3.0);
  color += sky_color * rim * mix(k_readable_rim_near, k_readable_rim_far, zoom);

  if (material_id == 2) {
    vec3 half_vector = normalize(light_dir + view_dir);
    float metal_glint = pow(max(dot(surface_normal, half_vector), 0.0), 28.0);
    color += sun_color * metal_glint * 0.18 * environment_primary_intensity();
  }
  return clamp(color, 0.0, 1.0);
}

void main() {
  vec3 base = u_color;
  if (v_color_role > 0 && v_color_role <= u_role_color_count) {
    base = u_role_colors[v_color_role - 1];
  }
  if (u_use_texture) {
    base *= texture(u_texture, v_tex).rgb;
  }
  float zoom = readable_zoom(v_pos_ws);

  vec4 readable_wear = v_wear_params;
  readable_wear.x *= mix(1.0, k_readable_wear_far, zoom);
  readable_wear.y *= mix(1.0, k_readable_grime_far, zoom);
  readable_wear.z *= mix(1.0, k_readable_blood_far, zoom);
  base = apply_wear(base, u_material_id, v_color_role, v_pos_local, readable_wear);

  vec3 surface_normal = normalize(v_normal_ws);
  vec3 light_dir = environment_primary_direction();
  vec3 sun_color = environment_primary_color();
  vec3 sky_color = environment_sky_color();
  vec3 color =
      shade_readable_character(base, surface_normal, v_pos_ws, u_material_id, zoom);
  color = apply_directional_shadow(color, v_pos_ws, surface_normal);

  color = max(color, base * sky_color * k_readable_shadow_floor);
  if (u_material_id == k_wildlife_material) {
    vec3 view_dir = normalize(u_camera_position - v_pos_ws);
    float skylight = clamp(surface_normal.y * 0.5 + 0.5, 0.0, 1.0);
    float edge = pow(1.0 - max(dot(surface_normal, view_dir), 0.0), 3.0);
    float sheen =
        pow(max(dot(normalize(surface_normal + vec3(0.0, 0.30, 0.12)), light_dir), 0.0),
            9.0);
    color += sun_color * sheen * 0.09;
    color -= sky_color * edge * mix(k_readable_rim_near, k_readable_rim_far, zoom);
    color += sun_color * edge * k_wildlife_rim;
    color = mix(color * vec3(1.07, 1.00, 0.85), color, skylight);
    color = max(color, vec3(0.0));
  }
  if (u_material_id == 6) {
    bool horse_hair = v_color_role == 5 || v_color_role == 6;
    bool dark_detail = v_color_role == 4 || v_color_role == 8;
    float sheen =
        pow(max(dot(normalize(surface_normal + vec3(0.0, 0.28, 0.14)), light_dir), 0.0),
            horse_hair ? 12.0 : 7.0);
    float skylight = clamp(surface_normal.y * 0.5 + 0.5, 0.0, 1.0);
    color += sun_color * sheen * (horse_hair ? 0.14 : 0.06);
    color += sky_color * (1.0 - skylight) * (horse_hair ? 0.04 : 0.02);
    if (dark_detail) {
      color = min(color, base * 1.10 + vec3(0.015));
    }
  }
  color = apply_zoom_readability(color, zoom);
  frag_color = vec4(color, u_alpha);
}
