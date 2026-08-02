#version 330 core
#include "directional_shadows.glsl"
#include "environment_lighting.glsl"
#include "local_lighting.glsl"
#include "noise.glsl"
#include "visibility_mask.glsl"

in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_uv;
in float v_vertex_displacement;
in float v_entry_mask;
in float v_feature_foot;

layout(location = 0) out vec4 frag_color;

uniform vec3 u_grass_primary, u_grass_secondary, u_grass_dry, u_soil_color;
uniform vec3 u_rock_low, u_rock_high, u_tint;
uniform int u_ground_type;
uniform int u_terrain_type;
uniform vec2 u_noise_offset;
uniform float u_tile_size, u_macro_noise_scale, u_detail_noise_scale;
uniform float u_slope_rock_threshold, u_slope_rock_sharpness;
uniform float u_soil_blend_height, u_soil_blend_sharpness;
uniform float u_height_noise_strength, u_height_noise_frequency;
uniform float u_ambient_boost, u_rock_detail_strength;

uniform float u_snow_coverage;
uniform float u_moisture_level;
uniform float u_crack_intensity;
uniform float u_rock_exposure;
uniform float u_grass_saturation;
uniform float u_soil_roughness;
uniform float u_curvature_response;
uniform float u_ridge_response;
uniform float u_gully_response;
uniform vec3 u_snow_color;

uniform float u_soil_foot_height;

uniform int u_has_height_tex;
uniform sampler2D u_height_tex;
uniform vec2 u_height_texel_size;
uniform vec2 u_height_uv_scale, u_height_uv_offset;
uniform float u_height_tex_to_world;

uniform float u_screen_toe_mul;
uniform float u_screen_toe_clamp;
uniform vec3 u_camera_pos;
uniform float u_fog_start;
uniform float u_fog_end;

float noise21(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  float a = soi_hash21_8b0317(i);
  float b = soi_hash21_8b0317(i + vec2(1.0, 0.0));
  float c = soi_hash21_8b0317(i + vec2(0.0, 1.0));
  float d = soi_hash21_8b0317(i + vec2(1.0, 1.0));
  vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

vec2 hash22(vec2 p) {
  vec2 h = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
  return -1.0 + 2.0 * fract(sin(h) * 43758.5453123);
}

float gradient_noise(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
  float a = dot(hash22(i), f);
  float b = dot(hash22(i + vec2(1.0, 0.0)), f - vec2(1.0, 0.0));
  float c = dot(hash22(i + vec2(0.0, 1.0)), f - vec2(0.0, 1.0));
  float d = dot(hash22(i + vec2(1.0, 1.0)), f - vec2(1.0, 1.0));
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y) * 1.55;
}

float gradient_fbm(vec2 p) {
  float value = 0.0;
  float amplitude = 0.54;
  mat2 octave_rotation = mat2(0.80, -0.60, 0.60, 0.80);
  for (int i = 0; i < 5; ++i) {
    value += gradient_noise(p) * amplitude;
    p = octave_rotation * p * 2.03 + vec2(7.1, -3.8);
    amplitude *= 0.49;
  }
  return value;
}

vec2 cellular_distances(vec2 p) {
  vec2 cell = floor(p);
  vec2 local = fract(p);
  float nearest = 8.0;
  float second = 8.0;
  for (int y = -1; y <= 1; ++y) {
    for (int x = -1; x <= 1; ++x) {
      vec2 offset = vec2(float(x), float(y));
      vec2 point = 0.5 + 0.46 * hash22(cell + offset);
      float distance_to_point = length(offset + point - local);
      if (distance_to_point < nearest) {
        second = nearest;
        nearest = distance_to_point;
      } else if (distance_to_point < second) {
        second = distance_to_point;
      }
    }
  }
  return vec2(nearest, second);
}

float curvature_from_normal_field(vec3 shading_normal) {
  vec3 dpdx = dFdx(v_world_pos);
  vec3 dpdy = dFdy(v_world_pos);
  float span_x = max(length(dpdx), 1e-4);
  float span_y = max(length(dpdy), 1e-4);
  vec3 dndx = dFdx(shading_normal);
  vec3 dndy = dFdy(shading_normal);
  float curve_x = dot(dndx, dpdx / span_x) / span_x;
  float curve_y = dot(dndy, dpdy / span_y) / span_y;
  return -0.5 * (curve_x + curve_y);
}

float compute_curvature() {
  if (u_has_height_tex != 1) {
    return 0.0;
  }
  vec2 uv = v_world_pos.xz * u_height_uv_scale + u_height_uv_offset;
  vec2 du = vec2(u_height_texel_size.x * 2.0, 0.0);
  vec2 dv = vec2(0.0, u_height_texel_size.y * 2.0);
  float center = texture(u_height_tex, uv).r * u_height_tex_to_world;
  float left = texture(u_height_tex, uv - du).r * u_height_tex_to_world;
  float right = texture(u_height_tex, uv + du).r * u_height_tex_to_world;
  float down = texture(u_height_tex, uv - dv).r * u_height_tex_to_world;
  float up = texture(u_height_tex, uv + dv).r * u_height_tex_to_world;
  vec2 world_span = abs(vec2(du.x, dv.y) / max(abs(u_height_uv_scale), vec2(1e-6)));
  float curve_x =
      (left + right - 2.0 * center) / max(world_span.x * world_span.x, 1e-5);
  float curve_z = (down + up - 2.0 * center) / max(world_span.y * world_span.y, 1e-5);
  return 0.5 * (curve_x + curve_z);
}

vec3 geom_normal() {
  vec3 dx = dFdx(v_world_pos);
  vec3 dy = dFdy(v_world_pos);
  vec3 n = normalize(cross(dx, dy));
  return (dot(n, v_normal) < 0.0) ? -n : n;
}

float band_limit(float texels_per_pixel, float frequency) {
  return 1.0 - smoothstep(0.30, 0.85, texels_per_pixel * frequency);
}

vec3 relief_octave(vec2 coord, float footprint, float frequency, float step_size) {
  float fade = band_limit(footprint, frequency);
  if (fade <= 0.001) {
    return vec3(0.0);
  }
  float h0 = gradient_noise(coord * frequency);
  float hx = gradient_noise((coord + vec2(step_size, 0.0)) * frequency);
  float hz = gradient_noise((coord + vec2(0.0, step_size)) * frequency);
  return vec3(vec2(hx - h0, hz - h0) / step_size, fade);
}

float sample_height(vec2 uv) {
  return texture(u_height_tex, uv).r * u_height_tex_to_world;
}

vec2 uv_to_world(vec2 duv) {
  return duv / max(abs(u_height_uv_scale), vec2(1e-6));
}

vec3 heightmap_normal(vec2 uv) {

  const float normal_span = 2.0;
  vec2 du = vec2(u_height_texel_size.x * normal_span, 0.0);
  vec2 dv = vec2(0.0, u_height_texel_size.y * normal_span);

  float h_l = sample_height(uv - du);
  float h_r = sample_height(uv + du);
  float h_d = sample_height(uv - dv);
  float h_u = sample_height(uv + dv);

  float dx_w = max(1e-6, abs(uv_to_world(du).x));
  float dz_w = max(1e-6, abs(uv_to_world(dv).y));

  float dhdx = (h_r - h_l) / (2.0 * dx_w);
  float dhdz = (h_u - h_d) / (2.0 * dz_w);

  return normalize(vec3(-dhdx, 1.0, -dhdz));
}

float tactical_zoom() {
  return smoothstep(58.0, 115.0, length(u_camera_pos - v_world_pos));
}

void main() {
  float tactical = tactical_zoom();
  float biome_forest = float(u_ground_type == 0);
  float biome_dry = float(u_ground_type == 1);
  float biome_rocky = float(u_ground_type == 2);
  float biome_alpine = float(u_ground_type == 3);
  float biome_fertile = float(u_ground_type == 4);
  float mountain_surface = float(u_terrain_type == 2);

  float entry_mask = clamp(v_entry_mask, 0.0, 1.0);
  float feature_foot = clamp(v_feature_foot, 0.0, 1.0);

  vec3 smooth_normal = normalize(v_normal);
  vec3 facet_normal = geom_normal();
  float facet_break = 1.0 - clamp(dot(facet_normal, smooth_normal), 0.0, 1.0);
  float facet_weight = 0.20 * smoothstep(0.30, 0.80, facet_break);
  vec3 normal = normalize(mix(smooth_normal, facet_normal, facet_weight));

  if (u_has_height_tex == 1) {
    vec2 huv = v_world_pos.xz * u_height_uv_scale + u_height_uv_offset;
    vec3 hm_n = heightmap_normal(huv);
    float slope0 = 1.0 - clamp(normal.y, 0.0, 1.0);

    float w = 0.18 * (1.0 - 0.24 * smoothstep(0.78, 0.98, slope0));
    w *= (1.0 - 0.38 * entry_mask - 0.12 * feature_foot);
    normal = normalize(mix(normal, hm_n, w));
  }

  float slope = 1.0 - clamp(normal.y, 0.0, 1.0);
  slope *= (1.0 - 0.25 * entry_mask);
  slope *= (1.0 - 0.10 * feature_foot);
  float entry_shelter = entry_mask * (1.0 - smoothstep(0.18, 0.55, slope));
  float entry_core = smoothstep(0.42, 0.82, entry_mask);
  float foot_shelter = feature_foot * (1.0 - smoothstep(0.34, 0.74, slope));
  float entry_toe =
      entry_mask * (1.0 - smoothstep(0.24, 0.62, slope * (1.0 - 0.25 * entry_mask)));
  float entry_face = entry_core * smoothstep(0.045, 0.16, slope) *
                     (1.0 - smoothstep(0.58, 0.82, slope));
  float entry_signal = entry_core * (0.30 + 0.70 * smoothstep(0.025, 0.14, slope));
  float curvature = (u_has_height_tex == 1)
                        ? compute_curvature()
                        : curvature_from_normal_field(smooth_normal);
  float curvature_response = clamp(u_curvature_response, 0.0, 1.0);
  float ridge_response = clamp(u_ridge_response, 0.0, 1.0);
  float gully_response = clamp(u_gully_response, 0.0, 1.0);
  float curvature_gain = mix(1.0, 2.2, curvature_response);
  float ridge_threshold = mix(0.02, 0.008, ridge_response);
  float gully_threshold = mix(0.02, 0.008, gully_response);
  float ridge_mask =
      smoothstep(0.0, ridge_threshold, max(0.0, curvature * curvature_gain));
  float gully_mask =
      smoothstep(0.0, gully_threshold, max(0.0, -curvature * curvature_gain));

  float tile_scale = max(u_tile_size, 0.0001);
  vec2 world_coord = (v_world_pos.xz / tile_scale) + u_noise_offset;

  float coord_footprint = max(length(fwidth(world_coord)), 1e-5);

  float wall_axis = step(abs(normal.x), abs(normal.z));
  vec2 wall_coord = vec2(mix(v_world_pos.z, v_world_pos.x, wall_axis) / tile_scale,
                         v_world_pos.y / tile_scale);
  wall_coord += u_noise_offset.yx;

  float macro_scale = max(u_macro_noise_scale, 0.010);
  float detail_scale = max(u_detail_noise_scale, 0.045);
  vec2 domain_warp = vec2(gradient_fbm(world_coord * macro_scale * 0.43 + 13.7),
                          gradient_fbm(world_coord * macro_scale * 0.43 - 9.2));
  float regional_field = clamp(
      0.5 + gradient_fbm(world_coord * macro_scale * 0.56 + domain_warp * 0.85) * 0.72,
      0.0,
      1.0);
  float soil_field = clamp(
      0.5 + gradient_fbm(world_coord * macro_scale * 4.80 + domain_warp * 0.65 + 31.0) *
                0.72,
      0.0,
      1.0);
  float moisture_field = clamp(
      0.5 + gradient_fbm(world_coord * macro_scale * 1.80 - domain_warp * 0.60 + 73.0) *
                0.68,
      0.0,
      1.0);
  float meadow_field =
      clamp(0.5 + gradient_fbm(world_coord * macro_scale * 1.05 + domain_warp * 0.48 +
                               vec2(-47.0, 26.0)) *
                      0.70,
            0.0,
            1.0);
  float thatch_field =
      clamp(0.5 + gradient_fbm(world_coord * macro_scale * 2.60 - domain_warp * 0.32 +
                               vec2(21.0, -39.0)) *
                      0.66,
            0.0,
            1.0);
  float surface_detail =
      gradient_fbm(world_coord * detail_scale * 2.8 + vec2(5.7, -2.1));
  float grain_frequency = detail_scale * 11.0;
  float granular_frequency = detail_scale * 31.0;
  float speck_frequency = detail_scale * 92.0;
  float grain_fade = band_limit(coord_footprint, grain_frequency);
  float granular_fade = band_limit(coord_footprint, granular_frequency);
  float speck_fade = band_limit(coord_footprint, speck_frequency);
  float surface_grain =
      gradient_noise(world_coord * grain_frequency + vec2(-17.0, 8.0)) * grain_fade;
  float granular = gradient_noise(world_coord * granular_frequency + vec2(42.0, 19.0)) *
                   granular_fade;
  float speckle =
      gradient_fbm(world_coord * speck_frequency + vec2(-63.0, 24.0)) * speck_fade;

  surface_grain *= mix(1.0, 0.62, tactical);
  granular *= mix(1.0, 0.68, tactical);
  speckle *= mix(1.0, 0.58, tactical);

  float high_ground = smoothstep(0.8, 4.8, v_world_pos.y);
  float low_ground = 1.0 - smoothstep(0.45, 2.6, v_world_pos.y);
  float drainage_field = clamp(moisture_field * 0.60 + (1.0 - meadow_field) * 0.12 +
                                   gully_mask * 0.22 + low_ground * 0.10 - slope * 0.24,
                               0.0,
                               1.0);
  float exposure_field = clamp(soil_field * 0.44 + (1.0 - moisture_field) * 0.20 +
                                   meadow_field * 0.10 + high_ground * 0.10 +
                                   slope * 0.30 + ridge_mask * 0.12 - gully_mask * 0.18,
                               0.0,
                               1.0);
  float material_patch =
      clamp(0.5 + gradient_fbm(world_coord * macro_scale * 2.8 + domain_warp * 0.58 +
                               vec2(37.0, -61.0)) *
                      0.78,
            0.0,
            1.0);
  float grazed_patch = smoothstep(0.36,
                                  0.66,
                                  exposure_field * 0.54 + material_patch * 0.30 +
                                      (1.0 - drainage_field) * 0.16);
  float sparse_cover =
      smoothstep(0.32, 0.62, exposure_field * 0.55 + material_patch * 0.45);

  float grass_mix = 0.30 + regional_field * 0.36;
  vec3 grass_color = mix(u_grass_primary, u_grass_secondary, grass_mix);
  float green_excess = max(grass_color.g - max(grass_color.r, grass_color.b), 0.0);
  float fertile_green = smoothstep(0.08, 0.24, green_excess);
  grass_color *= mix(vec3(1.0), vec3(0.98, 0.90, 0.96), fertile_green);
  float exposed_ground = smoothstep(0.10, 0.42, slope) + ridge_mask * 0.20;
  float dry_patch = smoothstep(
      0.50, 0.76, exposure_field + biome_dry * 0.08 - u_moisture_level * 0.08);
  dry_patch = clamp(dry_patch + exposed_ground * 0.20, 0.0, 1.0);
  dry_patch *= (1.0 - gully_mask * 0.40);
  grass_color = mix(grass_color, u_grass_dry, dry_patch * 0.54);
  grass_color = mix(grass_color, u_grass_dry, sparse_cover * (0.10 + biome_dry * 0.10));
  float lush_patch = smoothstep(0.58, 0.82, drainage_field + u_moisture_level * 0.08);
  lush_patch *= 1.0 - smoothstep(0.16, 0.46, slope);
  grass_color = mix(grass_color, u_grass_secondary * 0.94, lush_patch * 0.18);
  float grass_weave = gradient_fbm(world_coord * 0.16 + domain_warp * 0.12 + 18.0);
  float grass_clumps = smoothstep(0.34, 0.76, thatch_field);

  float sward_drift = gradient_fbm(world_coord * macro_scale * 8.5 +
                                   domain_warp * 0.55 + vec2(63.0, -14.0));
  float graze_drift = gradient_fbm(world_coord * macro_scale * 22.0 -
                                   domain_warp * 0.40 + vec2(-28.0, 51.0));
  float tussock =
      gradient_fbm(world_coord * 0.90 + domain_warp * 0.20 + vec2(-8.0, 33.0)) *
      band_limit(coord_footprint, 0.90);

  grass_color *= 0.95 + regional_field * 0.045 + material_patch * 0.045 +
                 grass_clumps * 0.020 + surface_detail * 0.050 + grass_weave * 0.012 +
                 surface_grain * 0.070 + tussock * 0.040 + granular * 0.026 +
                 speckle * 0.012;

  vec3 cropped_sward = mix(u_grass_dry, u_grass_primary, 0.45);
  vec3 deep_sward = mix(u_grass_secondary, u_grass_primary, 0.30) * 0.88;
  grass_color =
      mix(grass_color, cropped_sward, grazed_patch * (0.12 + biome_dry * 0.06));
  grass_color = mix(
      grass_color, cropped_sward, clamp(sward_drift * 0.85 + 0.42, 0.0, 1.0) * 0.06);
  grass_color =
      mix(grass_color, deep_sward, clamp(-graze_drift * 0.95 + 0.34, 0.0, 1.0) * 0.05);
  grass_color *= 1.0 + sward_drift * 0.012 + graze_drift * 0.010;

  vec3 blade_shade = mix(u_grass_secondary, u_grass_dry, 0.35);
  grass_color = mix(grass_color, blade_shade, clamp(tussock, 0.0, 1.0) * 0.07);

  float damp_patch = smoothstep(0.68, 0.86, drainage_field + u_moisture_level * 0.10);
  damp_patch *= 1.0 - smoothstep(0.16, 0.48, slope);
  float bare_patch = smoothstep(0.39,
                                0.64,
                                exposure_field * 0.66 + material_patch * 0.28 +
                                    surface_detail * 0.06 - u_moisture_level * 0.05);
  bare_patch *= 1.0 - smoothstep(0.26, 0.60, slope);
  float worn_patch = smoothstep(0.46,
                                0.70,
                                exposure_field * 0.40 + grazed_patch * 0.30 +
                                    soil_field * 0.18 + (1.0 - moisture_field) * 0.12);
  worn_patch *= 1.0 - smoothstep(0.10, 0.30, slope);
  float soil_width = max(0.04, 1.0 / max(u_soil_blend_sharpness, 0.25));
  float lowland_soil = 1.0 - smoothstep(u_soil_blend_height - soil_width,
                                        u_soil_blend_height + soil_width,
                                        v_world_pos.y);

  float soil_mix = bare_patch * 0.70;
  soil_mix = max(soil_mix, sparse_cover * (0.24 + biome_dry * 0.18));
  soil_mix = max(soil_mix, worn_patch * 0.28);
  soil_mix = max(soil_mix,
                 worn_patch * (biome_dry * 0.14 + biome_rocky * 0.10 +
                               biome_forest * 0.06 + biome_fertile * 0.04));
  soil_mix = max(soil_mix, damp_patch * (0.22 + 0.14 * u_moisture_level));
  soil_mix = max(soil_mix, lowland_soil * (0.08 + 0.15 * u_moisture_level));
  float deposited_soil = gully_mask * (1.0 - smoothstep(0.08, 0.32, slope)) *
                         (0.14 + 0.14 * gully_response);
  soil_mix = max(soil_mix, deposited_soil);
  soil_mix = max(soil_mix, foot_shelter * (0.15 + u_soil_foot_height));
  float level_ground = 1.0 - smoothstep(0.018, 0.075, slope);
  soil_mix *= mix(1.0, 0.88, level_ground);
  float toe_soil =
      min(entry_toe * max(u_screen_toe_mul, 0.0), max(u_screen_toe_clamp, 0.0));
  soil_mix = max(soil_mix,
                 entry_signal * 0.26 + entry_face * 0.06 + entry_shelter * 0.03 +
                     entry_toe * 0.012 + toe_soil);
  soil_mix = clamp(soil_mix, 0.0, 0.78);
  vec3 ground_soil = mix(u_soil_color, u_grass_dry, level_ground * 0.30);
  vec3 varied_soil =
      ground_soil * (1.0 + surface_detail * 0.055 + surface_grain * 0.070 +
                     granular * 0.055 + speckle * 0.030);
  vec3 compacted_entry_earth = mix(u_soil_color, u_grass_dry, 0.18);
  varied_soil =
      mix(varied_soil, compacted_entry_earth, entry_signal * 0.34 + entry_face * 0.08);
  varied_soil *= 1.0 - damp_patch * u_moisture_level * 0.16;
  vec3 soil_blend = mix(grass_color, varied_soil, soil_mix);
  vec3 sediment_color = mix(u_soil_color, u_grass_dry, 0.30);
  soil_blend = mix(soil_blend,
                   sediment_color,
                   clamp(deposited_soil + foot_shelter * 0.12, 0.0, 1.0) * 0.24);
  float litter_patch = smoothstep(
      0.50, 0.74, thatch_field * 0.52 + soil_field * 0.30 + moisture_field * 0.18);
  vec3 litter_color = mix(u_soil_color * 0.70, u_grass_dry * 0.62, meadow_field);
  soil_blend = mix(soil_blend, litter_color, litter_patch * biome_forest * 0.25);
  float fertile_sward = smoothstep(0.42,
                                   0.72,
                                   moisture_field * 0.64 + meadow_field * 0.20 +
                                       (1.0 - soil_field) * 0.16);
  soil_blend = mix(soil_blend,
                   mix(u_grass_primary, u_grass_secondary, 0.68),
                   fertile_sward * biome_fertile * 0.16 * (1.0 - soil_mix));

  float rock_threshold = clamp(u_slope_rock_threshold, 0.08, 0.88);
  float rock_width = mix(0.19, 0.07, clamp(u_slope_rock_sharpness / 8.0, 0.0, 1.0));
  float rock_mask = smoothstep(rock_threshold, rock_threshold + rock_width, slope);
  float rock_breakup = gradient_fbm(world_coord * 0.19 + vec2(11.0, -23.0)) * 0.5 + 0.5;
  rock_mask = clamp(
      rock_mask + (rock_breakup - 0.54) * u_rock_detail_strength * 0.38, 0.0, 1.0);
  rock_mask = clamp(rock_mask + (u_rock_exposure - 0.3) * 0.20 +
                        ridge_mask * 0.16 * ridge_response,
                    0.0,
                    1.0);
  float weathered_exposure = smoothstep(0.34, 0.70, rock_breakup + ridge_mask * 0.10);
  rock_mask = clamp(rock_mask + biome_rocky * weathered_exposure * 0.12 +
                        biome_alpine * weathered_exposure * 0.09,
                    0.0,
                    1.0);
  float mountain_altitude = smoothstep(2.5, 10.0, v_world_pos.y);
  float mountain_face =
      mountain_surface * mountain_altitude * smoothstep(0.025, 0.14, slope);
  float mountain_shoulders = mountain_surface * smoothstep(7.0, 15.0, v_world_pos.y) *
                             smoothstep(0.012, 0.085, slope);
  rock_mask = max(rock_mask,
                  clamp(mountain_face * (0.70 + weathered_exposure * 0.25) +
                            mountain_shoulders * 0.28,
                        0.0,
                        0.96));
  rock_mask *= mix(weathered_exposure, 1.0, smoothstep(0.10, 0.34, slope));
  rock_mask *= (1.0 - 0.55 * entry_shelter) * (1.0 - 0.30 * foot_shelter);
  rock_mask *= smoothstep(0.010, 0.050, slope);
  rock_mask *= 1.0 - soil_mix * 0.55;

  float wall_blend = smoothstep(0.28, 0.72, slope);
  vec2 rock_coord = mix(world_coord, wall_coord, wall_blend);
  float rock_footprint = max(length(fwidth(rock_coord)), 1e-5);

  vec2 rock_cells = cellular_distances(rock_coord * 0.34 + vec2(8.0, -5.0));
  float fracture = 1.0 - smoothstep(0.025, 0.12, rock_cells.y - rock_cells.x);
  vec2 rock_chips = cellular_distances(rock_coord * 1.9 + vec2(-27.0, 14.0));
  float chipping = (1.0 - smoothstep(0.03, 0.16, rock_chips.y - rock_chips.x)) *
                   band_limit(rock_footprint, 1.9);
  float rock_detail = gradient_fbm(rock_coord * 0.62 + vec2(3.3, -11.0));
  float rock_grain = gradient_noise(rock_coord * 2.4 + vec2(-17.0, 8.0)) *
                     band_limit(rock_footprint, 2.4);
  float rock_value = clamp(0.44 + rock_detail * 0.44 + rock_grain * 0.20, 0.0, 1.0);
  vec3 rock_color = mix(u_rock_low, u_rock_high, rock_value);
  vec3 mountain_rock =
      mix(u_rock_low * 0.62, u_rock_high * 0.76, smoothstep(0.20, 0.86, rock_value));
  float mountain_material =
      clamp(mountain_face * 0.86 + mountain_shoulders * 0.48, 0.0, 0.94);
  rock_color = mix(rock_color, mountain_rock, mountain_material);
  float rock_strata =
      gradient_noise(vec2(rock_coord.x * 0.11 + v_world_pos.y * 0.32,
                          mix(world_coord.y, wall_coord.y, wall_blend) * 0.035));
  float bedding = gradient_noise(vec2(rock_coord.x * 0.06, v_world_pos.y * 0.85));
  rock_color *= 1.0 + rock_strata * (0.145 + 0.075 * mountain_surface) +
                bedding * (0.120 + 0.085 * mountain_surface) * wall_blend;
  rock_color *= 1.0 - fracture * (0.115 + 0.150 * u_rock_detail_strength);
  rock_color *= 1.0 - chipping * (0.070 + 0.090 * u_rock_detail_strength);
  rock_color *= 1.0 + rock_grain * 0.075;

  float ledge = 1.0 - smoothstep(0.30, 0.68, slope);
  float scrub_field = gradient_fbm(rock_coord * 1.15 + vec2(-52.0, 17.0)) * 0.5 + 0.5;
  float scrub = smoothstep(0.52,
                           0.86,
                           scrub_field * 0.62 + fracture * 0.26 + ledge * 0.28 -
                               high_ground * 0.18);
  vec3 lichen = mix(u_grass_dry, u_grass_secondary, 0.55) * 0.62;
  rock_color = mix(rock_color, lichen, scrub * 0.34 * (1.0 - u_snow_coverage * 0.6));

  vec3 terrain_color = mix(soil_blend, rock_color, rock_mask);

  if (u_crack_intensity > 0.01) {
    vec2 crack_warp = vec2(gradient_noise(world_coord * 0.9 + vec2(4.0, -13.0)),
                           gradient_noise(world_coord * 0.9 + vec2(-21.0, 6.0)));
    vec2 crack_cells = cellular_distances(world_coord * 2.6 + crack_warp * 0.55);
    float vein = 1.0 - smoothstep(0.010, 0.075, crack_cells.y - crack_cells.x);
    float crack_pattern = vein * band_limit(coord_footprint, 2.6);
    crack_pattern *= (1.0 - slope * 0.8) * (0.35 + 0.65 * dry_patch);
    crack_pattern *= 1.0 - damp_patch * 0.75;
    float crack_darkening = 1.0 - crack_pattern * u_crack_intensity * 0.35;
    terrain_color *= crack_darkening;
  }

  if (u_ground_type == 3 && u_snow_coverage > 0.01) {
    float alpine_snow_large =
        clamp(0.5 + gradient_fbm(world_coord * 0.10 + domain_warp * 0.35 +
                                 vec2(123.0, 456.0)) *
                        0.78,
              0.0,
              1.0);
    float alpine_snow_small =
        clamp(0.5 + gradient_fbm(world_coord * 0.32 - domain_warp * 0.18 +
                                 vec2(-89.0, 201.0)) *
                        0.58,
              0.0,
              1.0);
    float alpine_snow_field = alpine_snow_large * 0.74 + alpine_snow_small * 0.26;
    float alpine_snow_accumulation = smoothstep(0.43, 0.60, alpine_snow_field);
    float alpine_snow_edge = smoothstep(0.36, 0.48, alpine_snow_field) *
                             (1.0 - smoothstep(0.60, 0.72, alpine_snow_field));
    alpine_snow_accumulation = max(alpine_snow_accumulation, alpine_snow_edge * 0.35);

    float alpine_snow_slope_retention = 1.0 - smoothstep(0.12, 0.42, slope);
    float alpine_snow_mask = clamp(alpine_snow_accumulation * u_snow_coverage * 1.55 *
                                       alpine_snow_slope_retention,
                                   0.0,
                                   0.92);
    vec3 alpine_snow_color =
        u_snow_color * (0.98 + surface_detail * 0.035 + surface_grain * 0.020);
    terrain_color = mix(terrain_color, alpine_snow_color, alpine_snow_mask);
  }

  if (u_snow_coverage > 0.01) {
    float snowline = 18.0 + (regional_field - 0.5) * 2.4 + surface_detail * 0.65;
    float snow_edge_noise =
        gradient_fbm(world_coord * macro_scale * 5.4 + vec2(-34.0, 57.0));
    float altitude_snow =
        smoothstep(snowline, snowline + 3.6, v_world_pos.y) + snow_edge_noise * 0.10;
    altitude_snow = clamp(altitude_snow, 0.0, 1.0);
    float snow_shelf = 1.0 - smoothstep(0.08, 0.30, slope);
    float crest_snow = smoothstep(0.42, 0.82, ridge_mask);
    float high_summit = smoothstep(snowline + 5.0, snowline + 9.0, v_world_pos.y);
    float snow_retention =
        max(0.025 + 0.62 * snow_shelf, crest_snow * (0.58 + 0.16 * snow_shelf));
    snow_retention = max(snow_retention, high_summit * 0.18);
    float wind_deposit =
        0.78 +
        0.22 * smoothstep(0.34, 0.72, moisture_field * 0.55 + material_patch * 0.45);
    float snow_mask = clamp(mountain_surface * altitude_snow * snow_retention *
                                wind_deposit * u_snow_coverage * 1.85,
                            0.0,
                            0.95);

    vec3 snow_tinted =
        u_snow_color * (0.97 + surface_detail * 0.045 + surface_grain * 0.025);
    terrain_color = mix(terrain_color, snow_tinted, snow_mask);
  }

  vec3 gray_level = vec3(dot(terrain_color, vec3(0.299, 0.587, 0.114)));
  float grounded_saturation = clamp(u_grass_saturation * 0.96, 0.0, 1.16);
  terrain_color = mix(gray_level, terrain_color, grounded_saturation);
  terrain_color *= vec3(1.01, 0.99, 0.99);

  float terrain_luma = dot(terrain_color, vec3(0.299, 0.587, 0.114));
  terrain_color =
      mix(terrain_color, mix(vec3(terrain_luma), terrain_color, 0.94), tactical);

  float wet_surface =
      damp_patch * soil_mix * max(u_moisture_level, environment_wetness());
  terrain_color *= 1.0 - u_moisture_level * 0.06 * (1.0 - rock_mask);
  terrain_color *= 1.0 - wet_surface * 0.15;
  terrain_color *= u_tint;

  vec3 L = environment_primary_direction();

  vec2 relief_coord = mix(world_coord, wall_coord, wall_blend);
  float relief_footprint = max(length(fwidth(relief_coord)), 1e-5);

  vec3 coarse_relief =
      relief_octave(relief_coord + vec2(-17.0, 8.0), relief_footprint, 1.40, 0.10);
  vec3 mid_relief =
      relief_octave(relief_coord + vec2(31.0, -19.0), relief_footprint, 4.30, 0.033);
  vec3 fine_relief =
      relief_octave(relief_coord + vec2(-9.0, 44.0), relief_footprint, 12.50, 0.011);

  vec2 relief_gradient = coarse_relief.xy * (0.30 * coarse_relief.z) +
                         mid_relief.xy * (0.20 * mid_relief.z) +
                         fine_relief.xy * (0.11 * fine_relief.z);
  float relief_resolved =
      0.30 * coarse_relief.z + 0.20 * mid_relief.z + 0.11 * fine_relief.z;
  float relief_lost = clamp(1.0 - relief_resolved / 0.61, 0.0, 1.0);

  float material_roughness =
      clamp(0.30 + u_soil_roughness * 0.48 + rock_mask * 0.18 - wet_surface * 0.30,
            0.18,
            0.96);
  float relief_amp = 0.055 + (0.055 + 0.060 * u_soil_roughness) * soil_mix +
                     0.07 * rock_mask + 0.025 * exposed_ground + 0.040 * bare_patch;
  relief_amp *= mix(1.0, 0.78, tactical);
  vec3 relief_offset =
      mix(vec3(relief_gradient.x, 0.0, relief_gradient.y),
          vec3(relief_gradient.x, relief_gradient.y, 0.0) * wall_axis +
              vec3(0.0, relief_gradient.y, relief_gradient.x) * (1.0 - wall_axis),
          wall_blend);
  vec3 detail_normal = normalize(normal - relief_offset * relief_amp);

  float ndl = max(dot(detail_normal, L), 0.0);
  float concavity =
      max(gully_mask * gully_response, smoothstep(-0.025, 0.01, -curvature) * 0.35);
  float ambient_occlusion = mix(1.0, 0.74, concavity * (1.0 - 0.55 * entry_mask));

  ambient_occlusion *= 1.0 - relief_lost * relief_amp * 0.30;
  ambient_occlusion *= 1.0 - 0.10 * smoothstep(0.20, 0.75, slope);

  vec3 sun_light = environment_primary_color() * environment_primary_intensity();
  vec3 ambient_term = ambient_occlusion * environment_ambient_light(detail_normal);

  float wet_glint = wet_surface * pow(max(dot(detail_normal, L), 0.0), 10.0) * 0.07;

  vec3 to_camera = u_camera_pos - v_world_pos;
  float view_distance = max(length(to_camera), 1e-4);
  vec3 view_dir = to_camera / view_distance;

  float grazing = pow(1.0 - clamp(dot(detail_normal, view_dir), 0.0, 1.0), 5.0);
  float sheen = grazing * (0.020 + 0.070 * u_moisture_level) *
                (1.0 - material_roughness) * (1.0 - rock_mask * 0.55);

  vec3 lit_color = terrain_color *
                   (ambient_term + sun_light * (ndl * 0.76 + wet_glint) + sheen) *
                   u_ambient_boost * environment_exposure();
  lit_color += terrain_color * local_lighting(v_world_pos, detail_normal);
  lit_color = apply_directional_shadow(lit_color, v_world_pos, detail_normal);
  lit_color = apply_visibility_memory(lit_color, v_world_pos.xz);
  float horizon_fog = smoothstep(0.20, 0.88, 1.0 - abs(view_dir.y));
  float fog_amount = atmospheric_fog_amount(
      view_distance, u_fog_start, u_fog_end, 0.72, 0.60 * horizon_fog);
  lit_color = mix(lit_color, environment_fog_color(), fog_amount);

  frag_color = vec4(clamp(lit_color, 0.0, 1.0), 1.0);
}
