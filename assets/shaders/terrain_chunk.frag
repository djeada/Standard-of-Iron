#version 330 core

in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_uv;
in float v_vertex_displacement;
in float v_entry_mask;

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

float hash21(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise21(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  float a = hash21(i);
  float b = hash21(i + vec2(1.0, 0.0));
  float c = hash21(i + vec2(0.0, 1.0));
  float d = hash21(i + vec2(1.0, 1.0));
  vec2 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(vec2 p) {
  float v = 0.0, a = 0.5;
  for (int i = 0; i < 5; ++i) {
    v += noise21(p) * a;
    p = p * 2.07 + 13.17;
    a *= 0.5;
  }
  return v;
}

float fbm_detail(vec2 p) {
  float v = 0.0, a = 0.5;
  for (int i = 0; i < 6; ++i) {
    v += noise21(p) * a;
    p = p * 2.13 + 7.89;
    a *= 0.52;
  }
  return v;
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
  vec3 normal = geom_normal();

  normal = normalize(mix(normal, normalize(v_normal), entry_mask * 0.5));

  if (u_has_height_tex == 1) {
    vec2 huv = v_world_pos.xz * u_height_uv_scale + u_height_uv_offset;
    vec3 hm_n = heightmap_normal(huv);
    float slope0 = 1.0 - clamp(normal.y, 0.0, 1.0);

    float w = 0.70 * (1.0 - smoothstep(0.70, 0.95, slope0));

    w *= (1.0 - 0.50 * entry_mask);
    normal = normalize(mix(normal, hm_n, w));
  }

  float slope = 1.0 - clamp(normal.y, 0.0, 1.0);
  float flat_terrain_mask = 1.0 - smoothstep(0.05, 0.18, slope);

  slope *= (1.0 - 0.25 * entry_mask);
  float entry_shelter = entry_mask * (1.0 - smoothstep(0.18, 0.55, slope));
  float entry_toe =
      entry_mask *
      (1.0 - smoothstep(0.24, 0.62, slope * (1.0 - 0.25 * entry_mask)));
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

  float macro_noise = fbm(world_coord * u_macro_noise_scale);
  float detail_noise =
      triplanar_noise(v_world_pos, u_detail_noise_scale * 2.5 / tile_scale);
  float erosion_noise = triplanar_noise(
      v_world_pos, u_detail_noise_scale * 4.0 / tile_scale + 17.0);
  float micro_variation = fbm_detail(world_coord * u_macro_noise_scale * 8.0);

  float patch_noise = fbm(world_coord * u_macro_noise_scale * 0.4);
  float moisture_var = smoothstep(0.3, 0.7, patch_noise);
  float lush_factor = smoothstep(0.2, 0.8, macro_noise);
  lush_factor = mix(lush_factor, moisture_var, 0.3);
  vec3 lush_grass = mix(u_grass_primary, u_grass_secondary, lush_factor);
  float dryness = clamp(0.55 * slope + 0.45 * detail_noise, 0.0, 1.0);
  dryness = clamp(dryness + ridge_mask * 0.12 * ridge_response -
                      gully_mask * 0.10 * gully_response,
                  0.0, 1.0);
  dryness = clamp(dryness - entry_shelter * (0.14 + 0.10 * u_moisture_level),
                  0.0, 1.0);
  dryness += moisture_var * 0.15;
  dryness = mix(dryness, dryness * 0.45 + moisture_var * 0.10,
                flat_terrain_mask * 0.80);

  float height_fade = smoothstep(0.0, 2.5, v_world_pos.y);
  float dryness_by_height = mix(dryness, dryness * 1.15, height_fade * 0.4);
  vec3 grass_color = mix(lush_grass, u_grass_dry, dryness_by_height);
  grass_color *= (1.0 + micro_variation * 0.08);
  grass_color = mix(grass_color, mix(u_grass_secondary, u_soil_color, 0.20),
                    entry_shelter * (0.10 + 0.10 * u_moisture_level));
  vec3 flat_grass_color =
      mix(u_grass_primary, u_grass_secondary, 0.18 + moisture_var * 0.22);
  grass_color = mix(grass_color, flat_grass_color, flat_terrain_mask * 0.45);

  float soil_width = max(0.01, 1.0 / max(u_soil_blend_sharpness, 0.001));

  float height_noise =
      (triplanar_noise(v_world_pos, max(0.0001, u_height_noise_frequency)) -
       0.5) *
      u_height_noise_strength;
  height_noise *= mix(1.0, 0.68, flat_terrain_mask);

  float toe_local = smoothstep(0.25, 0.9, slope);

  vec2 dxxz = dFdx(v_world_pos.xz);
  vec2 dyxz = dFdy(v_world_pos.xz);
  float px_world = max(length(dxxz), length(dyxz));
  float dh = max(abs(dFdx(v_world_pos.y)), abs(dFdy(v_world_pos.y)));
  float toe_ss = clamp((dh / max(px_world, 1e-6)) * u_screen_toe_mul, 0.0,
                       u_screen_toe_clamp);

  float toe_hm = 0.0;
  if (u_has_height_tex == 1) {
    vec2 huv = v_world_pos.xz * u_height_uv_scale + u_height_uv_offset;
    float d_w = min_cliff_distance_radial(huv, u_toe_tex_radius,
                                          max(1e-4, u_toe_height_delta));
    float max_search_w = avg_world_per_texel() * float(u_toe_tex_radius);
    if (d_w < 1e8) {
      toe_hm =
          smoothstep(max_search_w, 0.0, d_w) * clamp(u_toe_strength, 0.0, 1.0);
    }
  }

  float toe_proximity =
      max(toe_local, max(toe_hm, toe_ss / max(1e-6, u_soil_foot_height)));

  float concavity_lift =
      gully_mask * ((0.25 + 0.25 * gully_response) * u_soil_foot_height);

  float soil_height = u_soil_blend_height + height_noise + concavity_lift +
                      entry_toe * u_soil_foot_height * 0.45;
  float band_width =
      soil_width + u_soil_foot_height * max(toe_proximity, entry_toe * 0.85);

  float soil_mix = 1.0 - smoothstep(soil_height - band_width,
                                    soil_height + band_width, v_world_pos.y);
  soil_mix = clamp(soil_mix, 0.0, 1.0);
  soil_mix = max(soil_mix, entry_shelter * (0.10 + 0.18 * (1.0 - slope)));

  float mud_patch = fbm(world_coord * 0.08 + vec2(7.3, 11.2));
  mud_patch = smoothstep(
      0.57, 0.76, mud_patch + gully_mask * 0.10 + u_moisture_level * 0.08);
  mud_patch *= (1.0 - flat_terrain_mask * 0.18);
  soil_mix = max(soil_mix, mud_patch * 0.88 * (1.0 - slope * 0.55));
  soil_mix *= (1.0 - flat_terrain_mask * (0.34 + 0.06 * u_moisture_level));

  vec3 soil_blend = mix(grass_color, u_soil_color, soil_mix);

  float slope_cut =
      smoothstep(u_slope_rock_threshold, u_slope_rock_threshold + 0.02, slope);
  float rock_mask = clamp(pow(slope_cut, max(1.0, u_slope_rock_sharpness)) +
                              (erosion_noise - 0.5) * u_rock_detail_strength,
                          0.0, 1.0);
  rock_mask *= 1.0 - soil_mix * 0.75;

  rock_mask *= (1.0 - 0.50 * entry_shelter);
  rock_mask *= (1.0 - flat_terrain_mask * 0.85);

  float rock_lerp = clamp(0.35 + detail_noise * 0.65, 0.0, 1.0);
  vec3 rock_color = mix(u_rock_low, u_rock_high, rock_lerp);

  float rock_detail_variation = fbm_detail(world_coord * 0.15) * 0.5 + 0.5;
  rock_color *= mix(0.92, 1.08, rock_detail_variation);
  rock_color = mix(rock_color, rock_color * 1.12,
                   clamp(u_rock_detail_strength * 1.3, 0.0, 1.0));

  vec3 micro_normal = normal;
  float micro_detail_scale = u_detail_noise_scale * 8.0 / tile_scale;
  vec2 micro_offset = vec2(0.008, 0.0);
  float h0 = triplanar_noise(v_world_pos, micro_detail_scale);
  float hx = triplanar_noise(v_world_pos + vec3(micro_offset.x, 0.0, 0.0),
                             micro_detail_scale);
  float hz = triplanar_noise(v_world_pos + vec3(0.0, 0.0, micro_offset.x),
                             micro_detail_scale);
  vec3 micro_grad =
      vec3((hx - h0) / micro_offset.x, 0.0, (hz - h0) / micro_offset.x);
  float micro_amp = 0.18 * u_rock_detail_strength * (0.15 + 0.85 * slope);
  micro_amp *= mix(1.0, 0.72, flat_terrain_mask);
  micro_normal = normalize(normal + micro_grad * micro_amp);

  float fine_detail = triplanar_noise(v_world_pos, micro_detail_scale * 2.5);
  vec3 fine_normal_perturb =
      vec3((fine_detail - 0.5) * 0.03, 0.0,
           (triplanar_noise(v_world_pos + vec3(0.1, 0.0, 0.0),
                            micro_detail_scale * 2.5) -
            0.5) *
               0.03);
  micro_normal =
      normalize(micro_normal + fine_normal_perturb * (0.3 + 0.7 * rock_mask));

  float is_flat = 1.0 - smoothstep(0.10, 0.25, slope);
  float is_high = smoothstep(u_soil_blend_height + 0.5,
                             u_soil_blend_height + 1.5, v_world_pos.y);
  float plateau_factor = is_flat * is_high;
  float is_gully = gully_mask * (1.0 - smoothstep(0.25, 0.6, slope));
  float is_steep = smoothstep(0.3, 0.5, slope);
  float is_rim = ridge_mask;
  float rim_factor = is_steep * is_rim;

  rock_mask = clamp(rock_mask + rim_factor * (0.10 + 0.16 * ridge_response) -
                        plateau_factor * 0.06 -
                        is_gully * (0.08 + 0.12 * gully_response),
                    0.0, 1.0);

  rock_mask = clamp(rock_mask + (u_rock_exposure - 0.3) * 0.4, 0.0, 1.0);

  vec3 terrain_color = mix(soil_blend, rock_color, rock_mask);
  terrain_color = mix(terrain_color, soil_blend, entry_shelter * 0.12);

  if (u_crack_intensity > 0.01) {
    float crack_noise1 = noise21(world_coord * 8.0);
    float crack_noise2 = noise21(world_coord * 16.0 + vec2(42.0, 17.0));
    float crack_pattern = smoothstep(0.45, 0.50, crack_noise1) *
                          smoothstep(0.40, 0.55, crack_noise2);
    crack_pattern *= (1.0 - slope * 0.8);
    float crack_darkening = 1.0 - crack_pattern * u_crack_intensity * 0.35;
    terrain_color *= crack_darkening;
  }

  if (u_snow_coverage > 0.01) {
    float snow_noise = fbm(world_coord * 0.5 + vec2(123.0, 456.0));
    float snow_accumulation = smoothstep(0.3, 0.7, snow_noise);

    float slope_snow_reduction = 1.0 - smoothstep(0.15, 0.45, slope);

    float height_snow_bonus = smoothstep(-0.5, 1.5, v_world_pos.y) * 0.3;
    float snow_mask = clamp(snow_accumulation * slope_snow_reduction *
                                (u_snow_coverage + height_snow_bonus),
                            0.0, 1.0);

    vec3 snow_tinted = u_snow_color * (1.0 + detail_noise * 0.1);
    terrain_color = mix(terrain_color, snow_tinted, snow_mask * 0.85);
  }

  vec3 gray_level = vec3(dot(terrain_color, vec3(0.299, 0.587, 0.114)));
  terrain_color = mix(gray_level, terrain_color, u_grass_saturation);

  float moisture_effect = u_moisture_level;

  float wet_darkening = 1.0 - moisture_effect * 0.15 * (1.0 - rock_mask);
  terrain_color *= wet_darkening;

  float jitter_amp = 0.06 * (0.5 + u_soil_roughness * 0.5);
  jitter_amp *= (1.0 - 0.16 * flat_terrain_mask);
  float jitter =
      (hash21(world_coord * 0.27 + vec2(17.0, 9.0)) - 0.5) * jitter_amp;
  float brightness_var = (moisture_var - 0.5) * 0.08 * (1.0 - rock_mask);
  terrain_color *= (1.0 + jitter + brightness_var) * u_tint;

  vec3 L = normalize(u_light_dir);
  float ndl = max(dot(micro_normal, L), 0.0);

  float sky_occlusion =
      max(smoothstep(-0.03, 0.01, -curvature), gully_mask * gully_response);
  float ao = mix(1.0, 0.75 - 0.08 * gully_response,
                 sky_occlusion * (1.0 - slope * 0.5));

  float ambient = 0.32 * ao;
  float fresnel =
      pow(1.0 - max(dot(micro_normal, vec3(0.0, 1.0, 0.0)), 0.0), 2.2);

  float surface_roughness = mix(0.65, 0.95, u_soil_roughness);
  surface_roughness = mix(surface_roughness, 0.42, u_moisture_level * 0.5);

  vec3 view_dir = normalize(vec3(0.0, 1.0, -0.5));
  vec3 half_dir = normalize(L + view_dir);
  float spec_angle = max(dot(micro_normal, half_dir), 0.0);
  float specular = pow(spec_angle, mix(4.0, 32.0, 1.0 - surface_roughness));

  float spec_contrib =
      fresnel * 0.14 * (1.0 - surface_roughness) * (1.0 - rock_mask) * specular;

  spec_contrib += u_moisture_level * 0.10 * fresnel * (1.0 - rock_mask);
  float shade = ambient + ndl * 0.78 + spec_contrib;

  float plateau_muted = plateau_factor * (1.0 - 0.70 * entry_mask);
  float plateau_desat = clamp(0.75 * plateau_muted, 0.0, 0.75);
  float plateau_dim = clamp(0.25 * plateau_muted, 0.0, 0.25);

  float luma_p = dot(terrain_color, vec3(0.299, 0.587, 0.114));
  terrain_color = mix(terrain_color, vec3(luma_p), plateau_desat);

  float plateau_brightness = 1.0 - plateau_dim;
  float gully_darkness = 1.0 - is_gully * (0.04 + 0.07 * gully_response);
  float rim_contrast = 1.0 + rim_factor * (0.03 + 0.05 * ridge_response);

  {
    float track_strength =
        smoothstep(0.2, 0.7, u_moisture_level) * (1.0 - rock_mask * 0.9);
    float track1 = step(0.76, noise21(world_coord * 2.8 + vec2(3.17, 7.53)));
    float track2 = step(0.76, noise21(world_coord * 2.8 + vec2(11.4, 2.81)));
    float track3 = step(0.80, noise21(world_coord * 5.5 + vec2(19.2, 5.6)));
    float track_mask = max(max(track1, track2), track3) * track_strength;
    vec3 mud_color = mix(terrain_color * 0.68, vec3(0.30, 0.22, 0.14), 0.35);
    terrain_color = mix(terrain_color, mud_color, clamp(track_mask, 0.0, 0.55));
  }

  terrain_color *= plateau_brightness * gully_darkness * rim_contrast;
  vec3 lit_color = terrain_color * shade * u_ambient_boost;

  vec3 sun_tint = vec3(1.08, 0.94, 0.80);
  vec3 shadow_tint = vec3(0.78, 0.86, 1.04);
  float grade_t = clamp(ndl * 1.4, 0.0, 1.0);
  lit_color *= mix(shadow_tint, sun_tint, grade_t);

  vec3 to_camera = u_camera_pos - v_world_pos;
  float view_distance = max(length(to_camera), 1e-4);
  vec3 fog_view_dir = to_camera / view_distance;
  float distance_fog = smoothstep(
      u_fog_start, max(u_fog_start + 1e-4, u_fog_end), view_distance);
  float horizon_fog = smoothstep(0.20, 0.88, 1.0 - abs(fog_view_dir.y));
  float fog_amount =
      clamp(distance_fog * (0.72 + 0.60 * horizon_fog), 0.0, 1.0);
  lit_color = mix(lit_color, u_fog_color, fog_amount);

  frag_color = vec4(clamp(lit_color, 0.0, 1.0), 1.0);
}
