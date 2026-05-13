#version 330 core
in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_uv;
in float v_disp;
layout(location = 0) out vec4 frag_color;
uniform vec3 u_grass_primary;
uniform vec3 u_grass_secondary;
uniform vec3 u_grass_dry;
uniform vec3 u_soil_color;
uniform vec3 u_rock_low;
uniform vec3 u_rock_high;
uniform vec3 u_tint;
uniform vec2 u_noise_offset;
uniform float u_noise_angle;
uniform float u_tile_size;
uniform float u_macro_noise_scale;
uniform float u_detail_noise_scale;
uniform float u_soil_blend_height;
uniform float u_soil_blend_sharpness;
uniform float u_ambient_boost;
uniform vec3 u_light_dir;

uniform float u_snow_coverage;
uniform float u_moisture_level;
uniform float u_crack_intensity;
uniform float u_rock_exposure;
uniform float u_grass_saturation;
uniform float u_soil_roughness;
uniform float u_micro_bump_amp;
uniform float u_micro_bump_freq;
uniform float u_micro_normal_weight;
uniform float u_albedo_jitter;
uniform vec3 u_snow_color;
uniform vec3 u_camera_pos;
uniform vec3 u_fog_color;
uniform float u_fog_start;
uniform float u_fog_end;

float hash21(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}
float noise21(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  float a = hash21(i), b = hash21(i + vec2(1, 0)), c = hash21(i + vec2(0, 1)),
        d = hash21(i + vec2(1, 1));
  vec2 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}
float fbm(vec2 p) {
  float v = 0.0, a = 0.5;
  for (int i = 0; i < 3; ++i) {
    v += noise21(p) * a;
    p = p * 2.07 + 13.17;
    a *= 0.5;
  }
  return v;
}

mat2 rot2(float a) {
  float c = cos(a), s = sin(a);
  return mat2(c, -s, s, c);
}

void main() {
  vec3 n = normalize(v_normal);
  float ts = max(u_tile_size, 1e-4);
  float slope = 1.0 - clamp(n.y, 0.0, 1.0);
  float height_tint = clamp((v_world_pos.y + 0.35) * 0.55, -0.22, 0.30);

  vec2 base_uv = rot2(u_noise_angle) * ((v_world_pos.xz / ts) + u_noise_offset);
  vec2 warp_noise =
      vec2(fbm(base_uv * 0.22), fbm((base_uv + vec2(13.7, -6.1)) * 0.22));
  vec2 wuv = base_uv + (warp_noise - 0.5) * 1.1;

  float landform = fbm(wuv * max(u_macro_noise_scale * 0.13, 0.0025));
  float landform2 =
      fbm(wuv * max(u_macro_noise_scale * 0.25, 0.004) + vec2(-22.0, 8.0));
  float basin = smoothstep(0.18, 0.64, 1.0 - landform);
  float raised_shelf =
      smoothstep(0.54, 0.86, landform * 0.72 + landform2 * 0.28);
  float macro = fbm(wuv * u_macro_noise_scale);
  float detail = fbm(wuv * max(u_detail_noise_scale * 1.8, 0.12));
  float patch_noise = fbm(wuv * max(u_macro_noise_scale * 0.45, 0.006));
  float field_patch =
      fbm(wuv * max(u_macro_noise_scale * 0.20, 0.0035) + vec2(15.7, -31.4));
  float gravel_noise =
      fbm(wuv * max(u_detail_noise_scale * 3.2, 0.18) + vec2(5.2, 17.3));
  float pebble_noise = noise21(wuv * max(u_detail_noise_scale * 14.0, 0.9));
  float moisture_var = smoothstep(0.28, 0.72, patch_noise);
  float lush = smoothstep(0.18, 0.82, macro);
  lush = mix(lush, moisture_var, 0.34);
  lush = mix(lush, smoothstep(0.30, 0.76, field_patch), 0.20);
  float lowland = max(smoothstep(0.0, 0.34, -v_disp), basin * 0.38);
  float rise = max(smoothstep(0.10, 0.46, v_disp), raised_shelf * 0.26);
  float wind_scour =
      smoothstep(0.45, 0.82, detail + rise * 0.55 + slope * 0.30);

  vec3 lush_grass = mix(u_grass_primary, u_grass_secondary, lush);
  float dryness = 0.16 + detail * 0.15 + wind_scour * 0.38;
  dryness += raised_shelf * 0.16;
  dryness += (1.0 - u_moisture_level) * 0.18;
  dryness -= lowland * (0.16 + 0.26 * u_moisture_level);
  dryness = clamp(dryness, 0.0, 1.0);
  vec3 grass_col = mix(lush_grass, u_grass_dry, dryness);
  grass_col = mix(grass_col, u_grass_secondary, lowland * 0.16);
  float sw = max(0.01, 1.0 / max(u_soil_blend_sharpness, 1e-3));
  float s_n = (noise21(wuv * 4.0 + 9.7) - 0.5) * sw * 0.85;
  float soil_base = u_soil_blend_height + lowland * 0.08 - rise * 0.04;
  float soil_mix = 1.0 - smoothstep(soil_base - sw + s_n, soil_base + sw + s_n,
                                    v_world_pos.y);
  soil_mix = clamp(soil_mix, 0.0, 1.0);
  float mud_patch = fbm(wuv * 0.045 + vec2(7.3, 11.2));
  float mud_patch_fine = fbm(wuv * 0.11 + vec2(-8.4, 19.6));
  mud_patch =
      smoothstep(0.50, 0.70,
                 mud_patch * 0.72 + mud_patch_fine * 0.28 + basin * 0.20 +
                     lowland * 0.16 + u_moisture_level * 0.10);
  soil_mix = max(soil_mix, mud_patch * (0.76 + 0.22 * u_moisture_level));
  soil_mix = max(soil_mix, lowland * (0.25 + 0.20 * u_moisture_level));

  vec3 soil_col =
      mix(u_soil_color * 0.96, u_soil_color * 1.05, moisture_var * 0.55);
  vec3 base_col = mix(grass_col, soil_col, soil_mix);

  float gravel_mask = smoothstep(0.52, 0.86,
                                 gravel_noise * 0.72 + pebble_noise * 0.28 +
                                     wind_scour * 0.35 + dryness * 0.30);
  gravel_mask *= (0.12 + 0.88 * clamp(u_rock_exposure, 0.0, 1.0));
  gravel_mask *= 1.0 - lowland * 0.55;
  gravel_mask *= 1.0 - soil_mix * 0.35;
  vec3 rock_col = mix(u_rock_low, u_rock_high,
                      clamp(detail * 0.65 + pebble_noise * 0.35, 0.0, 1.0));
  rock_col *= mix(0.94, 1.08, gravel_noise);
  base_col = mix(base_col, rock_col, gravel_mask * 0.65);

  if (u_crack_intensity > 0.01) {
    float crack_noise1 = noise21(wuv * 8.0);
    float crack_noise2 = noise21(wuv * 16.0 + vec2(42.0, 17.0));
    float crack_pattern = smoothstep(0.45, 0.50, crack_noise1) *
                          smoothstep(0.40, 0.55, crack_noise2);
    crack_pattern *= (1.0 - lowland * 0.7) * (0.4 + 0.6 * dryness);
    float crack_darkening = 1.0 - crack_pattern * u_crack_intensity * 0.32;
    base_col *= crack_darkening;
  }

  if (u_snow_coverage > 0.01) {
    float snow_noise = fbm(wuv * 0.5 + vec2(123.0, 456.0));
    float snow_accumulation = smoothstep(0.3, 0.7, snow_noise);
    float height_snow_bonus = smoothstep(-0.5, 1.5, v_world_pos.y) * 0.3;
    float snow_mask = clamp(
        snow_accumulation * (u_snow_coverage + height_snow_bonus), 0.0, 1.0);
    vec3 snow_tinted = u_snow_color * (1.0 + detail * 0.1);
    base_col = mix(base_col, snow_tinted, snow_mask * 0.85);
  }

  vec3 gray_level = vec3(dot(base_col, vec3(0.299, 0.587, 0.114)));
  base_col = mix(gray_level, base_col, u_grass_saturation);

  float puddle_mask =
      lowland * (0.15 + 0.85 * u_moisture_level) * (1.0 - gravel_mask * 0.55);
  puddle_mask = clamp(puddle_mask, 0.0, 1.0);

  float wet_darkening = 1.0 - (u_moisture_level * 0.12 + puddle_mask * 0.10);
  base_col *= wet_darkening;

  float broad_breakup = fbm(wuv * 0.055 + vec2(31.0, -12.0));
  float broad_breakup2 = fbm(wuv * 0.018 + vec2(-41.0, 6.0));
  float damp_stain =
      smoothstep(0.50, 0.78,
                 broad_breakup * 0.62 + broad_breakup2 * 0.38 + lowland * 0.18 +
                     basin * 0.18 + u_moisture_level * 0.12);
  float dry_scuff =
      smoothstep(0.60, 0.86,
                 detail * 0.54 + patch_noise * 0.18 + field_patch * 0.22 +
                     dryness * 0.30 + raised_shelf * 0.14);
  vec3 damp_soil =
      mix(u_soil_color * 0.58, u_soil_color * 0.90, moisture_var * 0.65);
  vec3 dusty_grass = mix(u_grass_dry, u_soil_color, 0.34);
  float stain_weight = damp_stain * (0.12 + 0.28 * u_moisture_level) *
                       (1.0 - gravel_mask * 0.45);
  base_col = mix(base_col, damp_soil, stain_weight);
  base_col =
      mix(base_col, dusty_grass, dry_scuff * 0.18 * (1.0 - soil_mix * 0.35));

  vec3 dx = dFdx(v_world_pos);
  float m_scale =
      max(u_detail_noise_scale * (6.0 + u_micro_bump_freq * 2.5), 0.2);
  float h0 = fbm(wuv * m_scale);
  float hx = fbm((wuv + dFdx(wuv)) * m_scale);
  float hy = fbm((wuv + dFdy(wuv)) * m_scale);
  vec2 g = vec2(hx - h0, hy - h0);
  vec3 t = normalize(dx - n * dot(n, dx));
  vec3 b = normalize(cross(n, t));
  float relief_mask = clamp(0.35 + gravel_mask * 0.55 + soil_mix * 0.35 +
                                dry_scuff * 0.22 + damp_stain * 0.18,
                            0.0, 1.0);
  float micro_amp =
      clamp(u_micro_bump_amp, 0.02, 0.28) * mix(0.82, 1.55, relief_mask);
  vec3 micro_perturb = normalize(n - (t * g.x + b * g.y) * micro_amp);
  vec3 n_micro =
      normalize(mix(n, micro_perturb, clamp(u_micro_normal_weight, 0.0, 1.0)));
  float jitter_amp =
      max(0.01, u_albedo_jitter) * (0.65 + u_soil_roughness * 0.6);
  float jitter = (hash21(wuv * 0.27 + vec2(17.0, 9.0)) - 0.5) * jitter_amp;
  float speckle = step(0.74, noise21(wuv * 23.0 + vec2(2.0, 5.0)));
  float patch_brightness = (broad_breakup - 0.5) * 0.13 +
                           (broad_breakup2 - 0.5) * 0.10 +
                           (patch_noise - 0.5) * 0.06 + (landform - 0.5) * 0.08;
  float brightness_var = (moisture_var - 0.5) * 0.09 +
                         patch_brightness * (1.0 - puddle_mask * 0.45);
  vec3 col = base_col * (1.0 + jitter + brightness_var);
  col *= 1.0 + (speckle - 0.35) * 0.035 * (1.0 - puddle_mask);
  col *= u_tint;
  vec3 L = normalize(u_light_dir);
  float ndl = max(dot(n_micro, L), 0.0);
  float ambient = 0.38 + lowland * 0.04;
  float fres = pow(1.0 - max(dot(n_micro, vec3(0, 1, 0)), 0.0), 2.0);

  float surface_roughness =
      mix(0.58, 0.96, clamp(u_soil_roughness + gravel_mask * 0.25, 0.0, 1.0));
  surface_roughness = mix(surface_roughness, 0.34, u_moisture_level * 0.45);
  surface_roughness = mix(surface_roughness, 0.18, puddle_mask * 0.75);
  float spec_contrib = fres * 0.10 * (1.0 - surface_roughness);
  spec_contrib += u_moisture_level * 0.05 * fres;
  spec_contrib += puddle_mask * 0.10 * (0.4 + 0.6 * ndl);
  float shade = ambient + ndl * 0.65 + spec_contrib;
  vec3 lit = col * shade * (u_ambient_boost + height_tint);

  vec3 disp_tint = mix(vec3(0.72, 0.88, 1.02), vec3(1.06, 0.94, 0.82),
                       smoothstep(-0.12, 0.12, v_disp));
  lit = mix(lit, lit * disp_tint, 0.14);

  vec3 to_camera = u_camera_pos - v_world_pos;
  float view_distance = max(length(to_camera), 1e-4);
  vec3 fog_view_dir = to_camera / view_distance;
  float distance_fog = smoothstep(
      u_fog_start, max(u_fog_start + 1e-4, u_fog_end), view_distance);
  float horizon_fog = smoothstep(0.18, 0.85, 1.0 - abs(fog_view_dir.y));
  float fog_amount =
      clamp(distance_fog * (0.75 + 0.55 * horizon_fog), 0.0, 1.0);
  lit = mix(lit, u_fog_color, fog_amount);

  frag_color = vec4(clamp(lit, 0.0, 1.0), 1.0);
}
