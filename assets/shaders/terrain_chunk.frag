#version 330 core

in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_uv;
in float v_vertex_displacement;
in float v_entry_mask;
in float v_feature_foot;

layout(location = 0) out vec4 frag_color;

uniform vec3 u_grass_primary, u_grass_secondary, u_grass_dry, u_soil_color;
uniform vec3 u_rock_low, u_rock_high, u_tint, u_light_dir;
uniform vec2 u_noise_offset;
uniform float u_tile_size, u_macro_noise_scale;
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
uniform int u_toe_tex_radius;
uniform float u_toe_height_delta;
uniform float u_toe_strength;

uniform float u_screen_toe_mul;
uniform float u_screen_toe_clamp;
uniform vec3 u_camera_pos;
uniform vec3 u_fog_color;
uniform float u_fog_start;
uniform float u_fog_end;
uniform int u_has_visibility;
uniform sampler2D u_visibility_tex;
uniform vec2 u_visibility_size;
uniform float u_visibility_tile_size;
uniform float u_explored_alpha;

float hash21(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise21(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  float a = hash21(i);
  float b = hash21(i + vec2(1.0, 0.0));
  float c = hash21(i + vec2(0.0, 1.0));
  float d = hash21(i + vec2(1.0, 1.0));
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

float avg_world_per_texel() {
  vec2 wpt = abs(uv_to_world(u_height_texel_size));
  return 0.5 * (wpt.x + wpt.y);
}

float min_cliff_distance_radial(vec2 uv, int r, float rise_delta) {
  const int MAX_R = 12;
  const int NUM_DIR = 12;
  r = clamp(r, 1, MAX_R);

  float h0 = sample_height(uv);
  float best = 1e9;

  vec2 tex_step = u_height_texel_size;

  for (int d = 0; d < NUM_DIR; ++d) {
    float ang = 6.2831853 * (float(d) + 0.5) / float(NUM_DIR);
    vec2 dir = normalize(vec2(cos(ang), sin(ang))) * tex_step;

    vec2 p = uv;
    for (int s = 1; s <= MAX_R; ++s) {
      if (s > r)
        break;
      p += dir;

      float rise = sample_height(p) - h0;
      if (rise > rise_delta) {
        float step_world = length(uv_to_world(dir));
        float dist_world = step_world * float(s);
        best = min(best, dist_world);
        break;
      }
    }
  }
  return best;
}

void main() {
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

    float w = 0.90 * (1.0 - 0.24 * smoothstep(0.78, 0.98, slope0));
    w *= (1.0 - 0.38 * entry_mask - 0.12 * feature_foot);
    normal = normalize(mix(normal, hm_n, w));
  }

  float slope = 1.0 - clamp(normal.y, 0.0, 1.0);
  float flat_terrain_mask = 1.0 - smoothstep(0.05, 0.18, slope);

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
  float surface_detail = gradient_fbm(world_coord * 0.44 + vec2(5.7, -2.1));
  float grain_fade = band_limit(coord_footprint, 1.75);
  float granular_fade = band_limit(coord_footprint, 5.2);
  float speck_fade = band_limit(coord_footprint, 17.0);
  float surface_grain =
      gradient_noise(world_coord * 1.75 + vec2(-17.0, 8.0)) * grain_fade;
  float granular = gradient_noise(world_coord * 5.2 + vec2(42.0, 19.0)) * granular_fade;
  float speckle = gradient_fbm(world_coord * 17.0 + vec2(-63.0, 24.0)) * speck_fade;

  float grass_mix = 0.16 + regional_field * 0.68;
  vec3 grass_color = mix(u_grass_primary, u_grass_secondary, grass_mix);
  float green_excess = max(grass_color.g - max(grass_color.r, grass_color.b), 0.0);
  float fertile_green = smoothstep(0.08, 0.24, green_excess);
  grass_color *= mix(vec3(1.0), vec3(0.96, 0.78, 0.94), fertile_green);
  float high_ground = smoothstep(0.8, 4.8, v_world_pos.y);
  float exposed_ground = smoothstep(0.10, 0.42, slope) + ridge_mask * 0.20;
  float dry_patch =
      smoothstep(0.56,
                 0.78,
                 regional_field * 0.58 + meadow_field * 0.42 +
                     (0.5 - u_moisture_level) * 0.16 + high_ground * 0.08);
  dry_patch = clamp(dry_patch + exposed_ground * 0.20, 0.0, 1.0);
  dry_patch *= (1.0 - gully_mask * 0.40);
  grass_color = mix(grass_color, u_grass_dry, dry_patch * 0.62);
  float lush_patch = smoothstep(0.58,
                                0.78,
                                moisture_field * 0.68 + (1.0 - meadow_field) * 0.22 +
                                    u_moisture_level * 0.16 + gully_mask * 0.10);
  lush_patch *= 1.0 - smoothstep(0.16, 0.46, slope);
  grass_color = mix(grass_color, u_grass_secondary * 0.92, lush_patch * 0.24);
  float grass_weave = gradient_fbm(world_coord * 0.16 + domain_warp * 0.12 + 18.0);
  float grass_clumps = smoothstep(0.34, 0.76, thatch_field);

  float sward_drift = gradient_fbm(world_coord * macro_scale * 8.5 +
                                   domain_warp * 0.55 + vec2(63.0, -14.0));
  float graze_drift = gradient_fbm(world_coord * macro_scale * 22.0 -
                                   domain_warp * 0.40 + vec2(-28.0, 51.0));
  float tussock =
      gradient_fbm(world_coord * 0.90 + domain_warp * 0.20 + vec2(-8.0, 33.0)) *
      band_limit(coord_footprint, 0.90);

  grass_color *= 0.94 + grass_clumps * 0.09 + surface_detail * 0.040 +
                 grass_weave * 0.035 + surface_grain * 0.012 + tussock * 0.055 +
                 speckle * 0.014;

  vec3 cropped_sward = mix(u_grass_dry, u_grass_primary, 0.45);
  vec3 deep_sward = mix(u_grass_secondary, u_grass_primary, 0.30) * 0.88;
  grass_color = mix(
      grass_color, cropped_sward, clamp(sward_drift * 0.85 + 0.42, 0.0, 1.0) * 0.30);
  grass_color =
      mix(grass_color, deep_sward, clamp(-graze_drift * 0.95 + 0.34, 0.0, 1.0) * 0.24);
  grass_color *= 1.0 + sward_drift * 0.10 + graze_drift * 0.065;

  vec3 blade_shade = mix(u_grass_secondary, u_grass_dry, 0.35);
  grass_color = mix(grass_color, blade_shade, clamp(tussock, 0.0, 1.0) * 0.10);

  float low_ground = 1.0 - smoothstep(0.45, 2.6, v_world_pos.y);
  float damp_patch = smoothstep(0.75,
                                0.86,
                                moisture_field + u_moisture_level * 0.12 +
                                    gully_mask * 0.12 + low_ground * 0.05);
  damp_patch *= 1.0 - smoothstep(0.16, 0.48, slope);
  float bare_patch =
      smoothstep(0.57,
                 0.78,
                 soil_field * 0.74 + meadow_field * 0.20 + surface_detail * 0.08 +
                     exposed_ground * 0.12 - u_moisture_level * 0.04);
  bare_patch *= smoothstep(0.24, 0.64, 0.5 + surface_detail * 0.5);
  bare_patch *= 1.0 - smoothstep(0.26, 0.60, slope);

  float soil_mix = bare_patch * 0.52;
  soil_mix = max(soil_mix, damp_patch * (0.30 + 0.18 * u_moisture_level));
  soil_mix = max(soil_mix, gully_mask * (0.10 + 0.18 * gully_response));
  soil_mix = max(soil_mix, foot_shelter * 0.12);
  float level_ground = 1.0 - smoothstep(0.018, 0.075, slope);
  soil_mix *= mix(1.0, 0.88, level_ground);
  soil_mix = max(soil_mix,
                 entry_signal * 0.30 + entry_face * 0.08 + entry_shelter * 0.035 +
                     entry_toe * 0.015);
  soil_mix = clamp(soil_mix, 0.0, 0.72);
  vec3 ground_soil = mix(u_soil_color, u_grass_dry, level_ground * 0.24);
  vec3 varied_soil =
      ground_soil * (1.0 + surface_detail * 0.075 + surface_grain * 0.045 +
                     granular * 0.040 + speckle * 0.030);
  vec3 compacted_entry_earth = mix(u_soil_color, u_grass_dry, 0.18);
  varied_soil =
      mix(varied_soil, compacted_entry_earth, entry_signal * 0.38 + entry_face * 0.10);
  varied_soil *= 1.0 - damp_patch * u_moisture_level * 0.16;
  vec3 soil_blend = mix(grass_color, varied_soil, soil_mix);

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
  float rock_strata =
      gradient_noise(vec2(rock_coord.x * 0.11 + v_world_pos.y * 0.32,
                          mix(world_coord.y, wall_coord.y, wall_blend) * 0.035));
  float bedding = gradient_noise(vec2(rock_coord.x * 0.06, v_world_pos.y * 0.85));
  rock_color *= 1.0 + rock_strata * 0.145 + bedding * 0.120 * wall_blend;
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

  if (u_snow_coverage > 0.01) {
    float snow_accumulation = smoothstep(0.32, 0.72, regional_field);
    float slope_snow_reduction = 1.0 - smoothstep(0.18, 0.52, slope);
    float altitude_snow = smoothstep(6.0, 12.0, v_world_pos.y);
    float snow_mask = clamp(altitude_snow * (0.45 + 0.55 * snow_accumulation) *
                                slope_snow_reduction * u_snow_coverage * 1.20,
                            0.0,
                            0.84);

    vec3 snow_tinted = u_snow_color * (1.0 + surface_detail * 0.08);
    terrain_color = mix(terrain_color, snow_tinted, snow_mask * 0.85);
  }

  vec3 gray_level = vec3(dot(terrain_color, vec3(0.299, 0.587, 0.114)));
  terrain_color = mix(gray_level, terrain_color, u_grass_saturation);

  float wet_surface = damp_patch * soil_mix * u_moisture_level;
  terrain_color *= 1.0 - u_moisture_level * 0.06 * (1.0 - rock_mask);
  terrain_color *= 1.0 - wet_surface * 0.15;
  terrain_color *= u_tint;

  vec3 L = normalize(u_light_dir);

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

  float relief_amp = 0.020 + 0.090 * soil_mix + 0.30 * rock_mask +
                     0.030 * exposed_ground + 0.055 * bare_patch;
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

  vec3 sky_light = vec3(0.88, 0.94, 1.06);
  vec3 bounce_light = vec3(1.14, 0.98, 0.76);
  vec3 sun_light = vec3(1.10, 1.00, 0.86);
  float sky_access = 0.5 + 0.5 * detail_normal.y;
  vec3 ambient_term =
      0.40 * ambient_occlusion * mix(bounce_light, sky_light, sky_access);

  float wet_glint = wet_surface * pow(max(dot(detail_normal, L), 0.0), 10.0) * 0.07;

  vec3 to_camera = u_camera_pos - v_world_pos;
  float view_distance = max(length(to_camera), 1e-4);
  vec3 view_dir = to_camera / view_distance;

  float grazing = pow(1.0 - clamp(dot(detail_normal, view_dir), 0.0, 1.0), 5.0);
  float sheen = grazing * (0.035 + 0.075 * u_moisture_level) * (1.0 - rock_mask * 0.70);

  vec3 lit_color = terrain_color *
                   (ambient_term + sun_light * (ndl * 0.70 + wet_glint) + sheen) *
                   u_ambient_boost;
  float visibility_factor = 1.0;
  if (u_has_visibility == 1 && u_visibility_size.x > 0.0 && u_visibility_size.y > 0.0) {
    float tile_size = max(u_visibility_tile_size, 0.0001);
    vec2 grid = vec2(v_world_pos.x / tile_size, v_world_pos.z / tile_size);
    grid += (u_visibility_size * 0.5) - vec2(0.5);
    vec2 vis_uv = (grid + vec2(0.5)) / u_visibility_size;
    float vis_sample = texture(u_visibility_tex, vis_uv).r;
    if (vis_sample < 0.25) {
      discard;
    } else if (vis_sample < 0.75) {
      visibility_factor = u_explored_alpha;
    }
  }
  lit_color *= visibility_factor;
  float distance_fog =
      smoothstep(u_fog_start, max(u_fog_start + 1e-4, u_fog_end), view_distance);
  float horizon_fog = smoothstep(0.20, 0.88, 1.0 - abs(view_dir.y));
  float fog_amount = clamp(distance_fog * (0.72 + 0.60 * horizon_fog), 0.0, 1.0);
  lit_color = mix(lit_color, u_fog_color, fog_amount);

  frag_color = vec4(clamp(lit_color, 0.0, 1.0), 1.0);
}
