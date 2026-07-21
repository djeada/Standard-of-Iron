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

vec3 triplanar_weights(vec3 n) {
  vec3 b = abs(n);
  b = pow(b, vec3(4.0));
  return b / (b.x + b.y + b.z + 1e-5);
}

float triplanar_noise(vec3 wp, float s) {
  vec3 w = triplanar_weights(normalize(v_normal));
  float xy = noise21(wp.xy * s);
  float xz = noise21(wp.xz * s);
  float yz = noise21(wp.yz * s);
  return xy * w.z + xz * w.y + yz * w.x;
}

float compute_curvature() {
  float hx = dFdx(v_world_pos.y);
  float hy = dFdy(v_world_pos.y);
  return 0.5 * (dFdx(hx) + dFdy(hy));
}

vec3 geom_normal() {
  vec3 dx = dFdx(v_world_pos);
  vec3 dy = dFdy(v_world_pos);
  vec3 n = normalize(cross(dx, dy));
  return (dot(n, v_normal) < 0.0) ? -n : n;
}

float sample_height(vec2 uv) {
  return texture(u_height_tex, uv).r * u_height_tex_to_world;
}

vec2 uv_to_world(vec2 duv) {
  return duv / max(abs(u_height_uv_scale), vec2(1e-6));
}

vec3 heightmap_normal(vec2 uv) {

  vec2 du = vec2(u_height_texel_size.x, 0.0);
  vec2 dv = vec2(0.0, u_height_texel_size.y);

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
  vec3 normal = geom_normal();

  normal = normalize(
      mix(normal, normalize(v_normal), max(entry_mask * 0.5, feature_foot * 0.22)));

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
  float foot_shelter = feature_foot * (1.0 - smoothstep(0.34, 0.74, slope));
  float entry_toe =
      entry_mask * (1.0 - smoothstep(0.24, 0.62, slope * (1.0 - 0.25 * entry_mask)));
  float curvature = compute_curvature();
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

  // Low-frequency fields decide where materials occur; they never directly
  // shade the surface. Fine gradient/cellular fields provide structure inside
  // each material at a stable world scale without cloudy color noise.
  float macro_scale = max(u_macro_noise_scale, 0.010);
  vec2 domain_warp = vec2(gradient_fbm(world_coord * macro_scale * 0.43 + 13.7),
                          gradient_fbm(world_coord * macro_scale * 0.43 - 9.2));
  float regional_field = clamp(
      0.5 + gradient_fbm(world_coord * macro_scale * 0.56 + domain_warp * 0.85) * 0.72,
      0.0,
      1.0);
  float soil_field = clamp(
      0.5 + gradient_fbm(world_coord * macro_scale * 3.20 + domain_warp * 0.65 + 31.0) *
                0.72,
      0.0,
      1.0);
  float moisture_field = clamp(
      0.5 + gradient_fbm(world_coord * macro_scale * 1.80 - domain_warp * 0.60 + 73.0) *
                0.68,
      0.0,
      1.0);
  float surface_detail = gradient_fbm(world_coord * 0.44 + vec2(5.7, -2.1));
  float surface_grain = gradient_noise(world_coord * 1.75 + vec2(-17.0, 8.0));
  float granular = gradient_noise(world_coord * 5.2 + vec2(42.0, 19.0));

  float grass_mix = 0.34 + (regional_field - 0.5) * 0.18;
  vec3 grass_color = mix(u_grass_primary, u_grass_secondary, grass_mix);
  float high_ground = smoothstep(0.8, 4.8, v_world_pos.y);
  float exposed_ground = smoothstep(0.10, 0.42, slope) + ridge_mask * 0.20;
  float dry_patch = smoothstep(0.72,
                               0.84,
                               regional_field + (0.5 - u_moisture_level) * 0.13 +
                                   high_ground * 0.08);
  dry_patch = clamp(dry_patch + exposed_ground * 0.20, 0.0, 1.0);
  dry_patch *= (1.0 - gully_mask * 0.40);
  grass_color = mix(grass_color, u_grass_dry, dry_patch * 0.27);
  grass_color *= 1.0 + surface_detail * 0.055 + surface_grain * 0.030;

  float low_ground = 1.0 - smoothstep(0.45, 2.6, v_world_pos.y);
  float damp_patch = smoothstep(0.75, 0.86,
                                moisture_field + u_moisture_level * 0.12 +
                                    gully_mask * 0.12 + low_ground * 0.05);
  damp_patch *= 1.0 - smoothstep(0.16, 0.48, slope);
  float bare_patch = smoothstep(0.70, 0.81,
                                soil_field + exposed_ground * 0.11 -
                                    u_moisture_level * 0.03);
  bare_patch *= 1.0 - smoothstep(0.26, 0.60, slope);

  float soil_mix = bare_patch * 0.38;
  soil_mix = max(soil_mix, damp_patch * (0.18 + 0.14 * u_moisture_level));
  soil_mix = max(soil_mix, gully_mask * (0.10 + 0.18 * gully_response));
  soil_mix = max(soil_mix, entry_shelter * 0.16);
  soil_mix = max(soil_mix, foot_shelter * 0.12);
  soil_mix = clamp(soil_mix, 0.0, 0.62);
  vec3 varied_soil = u_soil_color *
                     (1.0 + surface_detail * 0.09 + surface_grain * 0.07 +
                      granular * 0.035);
  varied_soil *= 1.0 - damp_patch * u_moisture_level * 0.16;
  vec3 soil_blend = mix(grass_color, varied_soil, soil_mix);

  float rock_threshold = clamp(u_slope_rock_threshold, 0.08, 0.88);
  float rock_width = mix(0.19, 0.07, clamp(u_slope_rock_sharpness / 8.0, 0.0, 1.0));
  float rock_mask = smoothstep(rock_threshold, rock_threshold + rock_width, slope);
  float rock_breakup =
      gradient_fbm(world_coord * 0.19 + vec2(11.0, -23.0)) * 0.5 + 0.5;
  rock_mask = clamp(rock_mask + (rock_breakup - 0.54) *
                                    u_rock_detail_strength * 0.38,
                    0.0,
                    1.0);
  rock_mask = clamp(rock_mask + (u_rock_exposure - 0.3) * 0.20 +
                              ridge_mask * 0.16 * ridge_response,
                    0.0,
                    1.0);
  rock_mask *= (1.0 - 0.55 * entry_shelter) * (1.0 - 0.30 * foot_shelter);
  rock_mask *= (1.0 - flat_terrain_mask);
  rock_mask *= 1.0 - soil_mix * 0.55;

  vec2 rock_cells = cellular_distances(world_coord * 0.34 + vec2(8.0, -5.0));
  float fracture = 1.0 - smoothstep(0.025, 0.12, rock_cells.y - rock_cells.x);
  float rock_value =
      clamp(0.48 + surface_detail * 0.31 + surface_grain * 0.12, 0.0, 1.0);
  vec3 rock_color = mix(u_rock_low, u_rock_high, rock_value);
  rock_color *= 1.0 - fracture * (0.12 + 0.10 * u_rock_detail_strength);
  rock_color *= 1.0 + granular * 0.045;

  vec3 terrain_color = mix(soil_blend, rock_color, rock_mask);

  if (u_crack_intensity > 0.01) {
    float crack_noise1 = noise21(world_coord * 4.0);
    float crack_noise2 = noise21(world_coord * 8.0 + vec2(42.0, 17.0));
    float crack_pattern =
        smoothstep(0.45, 0.50, crack_noise1) * smoothstep(0.40, 0.55, crack_noise2);
    crack_pattern *= (1.0 - slope * 0.8);
    float crack_darkening = 1.0 - crack_pattern * u_crack_intensity * 0.35;
    terrain_color *= crack_darkening;
  }

  if (u_snow_coverage > 0.01) {
    float snow_accumulation = smoothstep(0.32, 0.72, regional_field);
    float slope_snow_reduction = 1.0 - smoothstep(0.18, 0.52, slope);
    float altitude_snow = smoothstep(2.2, 8.5, v_world_pos.y);
    float snow_mask = clamp(altitude_snow * (0.45 + 0.55 * snow_accumulation) *
                                slope_snow_reduction * u_snow_coverage * 1.55,
                            0.0, 0.92);

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
  float micro_h0 = gradient_noise(world_coord * 1.75 + vec2(-17.0, 8.0));
  float hx = gradient_noise((world_coord + vec2(0.10, 0.0)) * 1.75 +
                            vec2(-17.0, 8.0));
  float hz = gradient_noise((world_coord + vec2(0.0, 0.10)) * 1.75 +
                            vec2(-17.0, 8.0));
  vec3 detail_normal = normalize(normal +
                                 vec3(hx - micro_h0, 0.0, hz - micro_h0) *
                                     (0.14 + 0.24 * rock_mask + 0.06 * exposed_ground));
  float ndl = max(dot(detail_normal, L), 0.0);
  float concavity = max(gully_mask * gully_response,
                        smoothstep(-0.025, 0.01, -curvature) * 0.35);
  float ambient_occlusion = mix(1.0, 0.78, concavity * (1.0 - 0.55 * entry_mask));
  float ambient = 0.41 * ambient_occlusion;
  float wet_glint = wet_surface * pow(max(dot(detail_normal, L), 0.0), 10.0) * 0.07;
  float shade = ambient + ndl * 0.68 + wet_glint;
  vec3 lit_color = terrain_color * shade * u_ambient_boost;

  vec3 sun_tint = vec3(1.08, 0.94, 0.80);
  vec3 shadow_tint = vec3(0.78, 0.86, 1.04);
  float grade_t = clamp(ndl * 1.4, 0.0, 1.0);
  lit_color *= mix(shadow_tint, sun_tint, grade_t);

  vec3 to_camera = u_camera_pos - v_world_pos;
  float view_distance = max(length(to_camera), 1e-4);
  vec3 fog_view_dir = to_camera / view_distance;
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
  float horizon_fog = smoothstep(0.20, 0.88, 1.0 - abs(fog_view_dir.y));
  float fog_amount = clamp(distance_fog * (0.72 + 0.60 * horizon_fog), 0.0, 1.0);
  lit_color = mix(lit_color, u_fog_color, fog_amount);

  frag_color = vec4(clamp(lit_color, 0.0, 1.0), 1.0);
}
