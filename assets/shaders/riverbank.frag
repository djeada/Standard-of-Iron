#version 330 core
#include "directional_shadows.glsl"
#include "environment_lighting.glsl"
#include "local_lighting.glsl"

out vec4 frag_color;

in vec2 tex_coord;
in vec3 world_pos;
in vec3 v_normal;

uniform float time;

uniform sampler2D u_visibility_tex;
uniform vec2 u_visibility_size;
uniform float u_visibility_tile_size;
uniform float u_explored_alpha;
uniform int u_has_visibility;

uniform float u_segment_visibility;
uniform int u_water_surface_kind;
uniform int u_ground_type;

uniform vec3 u_ground_color;
uniform vec3 u_grass_secondary;
uniform vec3 u_grass_dry;
uniform vec3 u_soil_color;
uniform vec3 u_rock_low;
uniform vec3 u_rock_high;
uniform vec3 u_snow_color;
uniform float u_moisture_level;
uniform float u_rock_exposure;
uniform float u_snow_coverage;
uniform float u_ambient_boost;
uniform vec3 u_camera_pos;

uniform float u_fog_start;
uniform float u_fog_end;

float saturate(float value) {
  return clamp(value, 0.0, 1.0);
}

vec3 saturate(vec3 value) {
  return clamp(value, vec3(0.0), vec3(1.0));
}

float hash21(vec2 point) {
  point = fract(point * vec2(123.34, 456.21));
  point += dot(point, point + 45.32);

  return fract(point.x * point.y);
}

float value_noise(vec2 point) {
  vec2 cell = floor(point);
  vec2 local = fract(point);

  local = local * local * local * (local * (local * 6.0 - 15.0) + 10.0);

  float a = hash21(cell);
  float b = hash21(cell + vec2(1.0, 0.0));
  float c = hash21(cell + vec2(0.0, 1.0));
  float d = hash21(cell + vec2(1.0, 1.0));

  return mix(mix(a, b, local.x), mix(c, d, local.x), local.y);
}

float fbm(vec2 point) {
  float result = 0.0;
  float amplitude = 0.52;

  const mat2 octave_rotation = mat2(0.80, -0.60, 0.60, 0.80);

  for (int octave = 0; octave < 5; ++octave) {
    result += value_noise(point) * amplitude;

    point = octave_rotation * point * 2.03 + vec2(7.1, -3.8);

    amplitude *= 0.48;
  }

  return result;
}

float band_limit(float texels_per_pixel, float frequency) {
  return 1.0 - smoothstep(0.30, 0.85, texels_per_pixel * frequency);
}

vec2 cellular_distances(vec2 point) {
  vec2 cell = floor(point);
  vec2 local = fract(point);
  float nearest = 8.0;
  float second = 8.0;
  for (int y = -1; y <= 1; ++y) {
    for (int x = -1; x <= 1; ++x) {
      vec2 offset = vec2(float(x), float(y));
      vec2 site = offset + vec2(hash21(cell + offset), hash21(cell + offset + 19.7));
      float distance_to_site = length(site - local);
      if (distance_to_site < nearest) {
        second = nearest;
        nearest = distance_to_site;
      } else if (distance_to_site < second) {
        second = distance_to_site;
      }
    }
  }
  return vec2(nearest, second);
}

vec3 safe_normalize(vec3 value, vec3 fallback) {
  float length_squared = dot(value, value);

  if (length_squared <= 0.000001) {
    return fallback;
  }

  return value * inversesqrt(length_squared);
}

vec3 procedural_detail_normal(vec3 base_normal, vec2 point, float strength) {
  const float sample_step = 0.08;

  float center = fbm(point);

  float dx = fbm(point + vec2(sample_step, 0.0)) - center;

  float dz = fbm(point + vec2(0.0, sample_step)) - center;

  vec3 detail_offset = vec3(-dx, 0.0, -dz) * strength;

  return safe_normalize(base_normal + detail_offset, base_normal);
}

float calculate_visibility() {
  if (u_has_visibility != 1 || u_visibility_size.x <= 0.0 ||
      u_visibility_size.y <= 0.0) {
    return 1.0;
  }

  float tile_size = max(u_visibility_tile_size, 0.0001);

  vec2 grid = world_pos.xz / tile_size;
  grid += u_visibility_size * 0.5 - vec2(0.5);

  vec2 visibility_uv = (grid + vec2(0.5)) / u_visibility_size;

  if (visibility_uv.x < 0.0 || visibility_uv.y < 0.0 || visibility_uv.x > 1.0 ||
      visibility_uv.y > 1.0) {
    discard;
  }

  float visibility = texture(u_visibility_tex, visibility_uv).r;

  if (visibility < 0.25) {
    discard;
  }

  if (visibility < 0.75) {
    return saturate(u_explored_alpha);
  }

  return 1.0;
}

void main() {

  float visibility_factor = calculate_visibility();

  float river = u_water_surface_kind == 0 ? 1.0 : 0.0;
  float biome_forest = float(u_ground_type == 0);
  float biome_dry = float(u_ground_type == 1);
  float biome_rocky = float(u_ground_type == 2);
  float biome_alpine = float(u_ground_type == 3);
  float biome_fertile = float(u_ground_type == 4);

  float wash_speed = mix(0.018, 0.052, river);
  vec2 world_uv = world_pos.xz * 0.22;
  vec2 slow_wash = vec2(time * wash_speed, -time * wash_speed * 0.35);

  float macro = fbm(world_uv * 0.42 + vec2(11.0, -7.0));
  float detail = fbm(world_uv * 1.65 + vec2(-3.0, 17.0));
  float grain = value_noise(world_uv * 8.5 + vec2(29.0));
  float deposit_field = fbm(world_uv * 0.78 + vec2(-24.0, 31.0));
  float edge_macro = fbm(world_uv * 0.48 + vec2(37.0, -11.0));
  float edge_detail = fbm(world_uv * 2.15 + slow_wash + vec2(-9.0, 24.0));

  float edge_shift = (edge_macro - 0.5) * 0.18 + (edge_detail - 0.5) * 0.055 +
                     (deposit_field - 0.5) * 0.055;
  float shore_t = saturate(tex_coord.x + edge_shift);

  float moisture = saturate(u_moisture_level);
  float rock_exposure = saturate(u_rock_exposure);
  float snow_coverage = saturate(u_snow_coverage);

  float mud_weight = biome_forest * 0.70 + biome_dry * 0.14 + biome_rocky * 0.12 +
                     biome_alpine * 0.07 + biome_fertile * 0.64;
  float sand_weight = biome_forest * 0.12 + biome_dry * 0.66 + biome_rocky * 0.18 +
                      biome_alpine * 0.08 + biome_fertile * 0.18;
  float stone_weight = biome_forest * 0.18 + biome_dry * 0.20 + biome_rocky * 0.70 +
                       biome_alpine * 0.85 + biome_fertile * 0.18;

  mud_weight *= 0.70 + smoothstep(0.34, 0.72, macro + moisture * 0.12) * 0.68;
  sand_weight *=
      0.68 + smoothstep(0.36, 0.72, deposit_field + (1.0 - moisture) * 0.12) * 0.66;
  stone_weight *= 0.66 + smoothstep(0.38, 0.74, detail + rock_exposure * 0.18) * 0.76;
  float sediment_weight_sum = max(mud_weight + sand_weight + stone_weight, 0.001);
  mud_weight /= sediment_weight_sum;
  sand_weight /= sediment_weight_sum;
  stone_weight /= sediment_weight_sum;

  float wetness = 1.0 - smoothstep(0.045, mix(0.28, 0.40, mud_weight), shore_t);
  float contact = 1.0 - smoothstep(0.004, 0.070, shore_t);
  float damp_band =
      smoothstep(0.02, 0.12, shore_t) * (1.0 - smoothstep(0.34, 0.62, shore_t));

  vec3 grass_primary = max(u_ground_color, vec3(0.025));
  vec3 grass_secondary = max(u_grass_secondary, vec3(0.025));
  vec3 grass_dry = max(u_grass_dry, vec3(0.025));
  vec3 soil = max(u_soil_color, vec3(0.025));
  vec3 rock_low = max(u_rock_low, vec3(0.025));
  vec3 rock_high = max(u_rock_high, vec3(0.025));

  vec3 terrain_grass =
      mix(grass_primary, grass_secondary, smoothstep(0.24, 0.76, macro));
  float terrain_dryness = saturate((1.0 - moisture) * 0.44 + (detail - 0.42) * 0.25);
  terrain_grass = mix(terrain_grass, grass_dry, terrain_dryness);
  float terrain_soil_mask =
      smoothstep(0.58, 0.82, macro * 0.55 + detail * 0.30 + deposit_field * 0.15);
  vec3 terrain_soil = mix(soil, grass_dry, 0.12 + (1.0 - moisture) * 0.18);
  float terrain_rock_mask =
      smoothstep(0.58, 0.84, detail + rock_exposure * 0.20) *
      (rock_exposure * 0.70 + biome_rocky * 0.18 + biome_alpine * 0.24);
  vec3 terrain_rock = mix(rock_low, rock_high, detail);

  vec3 map_ground = mix(terrain_grass, terrain_soil, terrain_soil_mask * 0.48);
  map_ground = mix(map_ground, terrain_rock, terrain_rock_mask * 0.58);
  float bank_snow = smoothstep(0.52, 0.76, macro + detail * 0.14) * snow_coverage;
  map_ground = mix(map_ground, max(u_snow_color, vec3(0.025)), bank_snow * 0.84);
  vec3 map_ground_gray = vec3(dot(map_ground, vec3(0.299, 0.587, 0.114)));
  map_ground = mix(map_ground_gray, map_ground, 0.72) * vec3(1.025, 0.96, 0.985);

  vec3 mud_color = soil * mix(vec3(0.70, 0.74, 0.68), vec3(0.58, 0.66, 0.58), moisture);
  mud_color = mix(mud_color, soil * vec3(0.90, 0.88, 0.78), shore_t);
  vec3 sand_color = mix(soil, grass_dry, 0.58) *
                    mix(vec3(0.90, 0.88, 0.76), vec3(1.09, 1.04, 0.88), 1.0 - moisture);
  vec3 shingle_color = mix(rock_low, rock_high, grain * 0.58 + detail * 0.42) *
                       mix(vec3(0.74, 0.77, 0.74), vec3(0.94, 0.93, 0.88), shore_t);

  float mud_zone = smoothstep(0.40 - mud_weight * 0.12,
                              0.66 - mud_weight * 0.08,
                              macro * 0.50 + (1.0 - detail) * 0.22 + edge_macro * 0.18 +
                                  moisture * 0.10);
  float shingle_zone = smoothstep(0.45 - stone_weight * 0.16,
                                  0.70 - stone_weight * 0.10,
                                  detail * 0.42 + grain * 0.20 + deposit_field * 0.24 +
                                      rock_exposure * 0.14);
  vec3 earth = mix(sand_color, mud_color, mud_zone * (0.30 + mud_weight * 0.70));
  earth = mix(earth, shingle_color, shingle_zone * (0.26 + stone_weight * 0.74));
  earth *= 0.88 + (macro - 0.5) * 0.16 + (detail - 0.5) * 0.09 + (grain - 0.5) * 0.045;

  float vegetation_strength = biome_forest * 0.82 + biome_dry * 0.26 +
                              biome_rocky * 0.18 + biome_alpine * 0.10 + biome_fertile;
  float grass_fingers =
      fbm(world_uv * 0.92 + vec2(-31.0, 6.0)) * 0.58 + edge_macro * 0.42;
  float grass_threshold = 0.58 - vegetation_strength * 0.10 + (shore_t - 0.42) * 0.50;
  float grass_patches =
      smoothstep(grass_threshold, grass_threshold + 0.18, grass_fingers + macro * 0.18);
  float grass_cover = smoothstep(0.025, 0.48, shore_t) * grass_patches *
                      vegetation_strength * (1.0 - stone_weight * 0.55);
  vec3 grass_color = terrain_grass * mix(0.80, 0.98, detail);
  earth = mix(earth, grass_color, saturate(grass_cover));

  float exposed_bank = smoothstep(0.34,
                                  0.62,
                                  deposit_field * 0.38 + macro * 0.24 +
                                      edge_macro * 0.22 + edge_detail * 0.16);
  exposed_bank = max(exposed_bank, mud_zone * mud_weight * 0.86);
  exposed_bank = max(exposed_bank, shingle_zone * (0.34 + stone_weight * 0.66));
  exposed_bank = max(exposed_bank, stone_weight * (0.54 + detail * 0.24));
  exposed_bank = max(exposed_bank, sand_weight * deposit_field * 0.72);
  earth = mix(map_ground, earth, 0.18 + exposed_bank * 0.82);

  float land_merge = smoothstep(
      0.38 + (edge_macro - 0.5) * 0.14, 0.88 + (deposit_field - 0.5) * 0.10, shore_t);
  earth = mix(earth, map_ground * mix(0.91, 1.01, detail), land_merge);

  float footprint = max(length(fwidth(world_uv)), 1e-5);
  vec2 cobble_cells = cellular_distances(world_uv * 4.6 + vec2(41.0, -13.0));
  float cobble =
      (1.0 - smoothstep(0.10, 0.45, cobble_cells.x)) * band_limit(footprint, 4.6);
  vec2 shingle_cells = cellular_distances(world_uv * 12.0 + vec2(-19.0, 8.0));
  float shingle =
      (1.0 - smoothstep(0.13, 0.49, shingle_cells.x)) * band_limit(footprint, 12.0);
  float bar_field =
      smoothstep(0.37, 0.71, deposit_field * 0.44 + macro * 0.22 + detail * 0.34);
  float pebble_region =
      smoothstep(0.03, 0.14, shore_t) * (1.0 - smoothstep(0.48, 0.78, shore_t));
  float pebbles = saturate(cobble * 0.58 + shingle * 0.42) * pebble_region *
                  mix(0.28, 0.96, bar_field) * mix(0.34, 1.0, stone_weight);
  vec3 stone_color = mix(rock_low, rock_high, grain * 0.66 + detail * 0.34);
  stone_color *= mix(0.66, 0.98, smoothstep(0.04, 0.50, shore_t));
  earth = mix(earth, stone_color, pebbles * (0.34 + stone_weight * 0.52));

  float waterline =
      contact * exposed_bank * (0.42 + 0.58 * smoothstep(0.34, 0.72, detail));
  earth *= 1.0 - waterline * (0.08 + mud_weight * 0.09);
  float stain =
      smoothstep(0.045, 0.095, shore_t) * (1.0 - smoothstep(0.11, 0.22, shore_t));
  earth = mix(earth, earth * vec3(0.76, 0.82, 0.78), stain * 0.38);

  float deposited_silt_noise =
      fbm(vec2(tex_coord.y * 7.0 + macro, world_uv.y * 0.35 - slow_wash.x));
  float deposited_silt = damp_band * smoothstep(0.60, 0.84, deposited_silt_noise);
  earth *= 1.0 - deposited_silt * (0.06 + sand_weight * 0.10);

  float shore_extent = biome_forest * 0.72 + biome_dry * 0.74 + biome_rocky * 0.88 +
                       biome_alpine * 0.91 + biome_fertile * 0.78 +
                       (1.0 - river) * 0.045;
  float transition_field =
      deposit_field * 0.40 + macro * 0.28 + edge_macro * 0.22 + detail * 0.10;
  float transition_coverage =
      smoothstep(0.36 + shore_t * 0.09, 0.57 + shore_t * 0.11, transition_field);

  vec3 base_normal = safe_normalize(v_normal, vec3(0.0, 1.0, 0.0));

  float relief_fade = band_limit(footprint, 2.8);
  float relief_strength =
      (0.42 * (0.35 + grain) + pebbles * 0.55 + cobble * 0.25) * relief_fade;
  vec3 normal = procedural_detail_normal(base_normal, world_uv * 2.8, relief_strength);

  vec3 light_dir = environment_primary_direction();

  vec3 view_dir = safe_normalize(u_camera_pos - world_pos, normal);

  float ndl = max(dot(normal, light_dir), 0.0);

  float ambient_occlusion = mix(0.82, 1.0, smoothstep(0.0, 0.38, shore_t));
  ambient_occlusion *= 1.0 - (1.0 - relief_fade) * pebbles * 0.30;

  vec3 sun_light = environment_primary_color() * environment_primary_intensity();
  vec3 ambient_term = ambient_occlusion * environment_ambient_light(normal);

  float exposure =
      environment_exposure() * (u_ambient_boost > 0.001 ? u_ambient_boost : 1.0);
  vec3 color = earth * (ambient_term + sun_light * ndl * 0.70) * exposure;

  vec3 half_dir = safe_normalize(light_dir + view_dir, normal);

  float specular_power = mix(62.0, 24.0, shore_t);

  float wet_specular = pow(max(dot(normal, half_dir), 0.0), specular_power);

  wet_specular *= wetness * (0.055 + contact * 0.12);

  vec3 wet_highlight = mix(map_ground, vec3(0.32, 0.37, 0.36), 0.28);

  color += wet_highlight * wet_specular;

  vec3 contact_color = color * vec3(0.68, 0.73, 0.70);

  color = mix(color, contact_color, contact * 0.28);

  float segment_visibility = saturate(u_segment_visibility);

  color *= visibility_factor * segment_visibility;

  float view_distance = length(u_camera_pos - world_pos);

  float fog_amount =
      atmospheric_fog_amount(view_distance, u_fog_start, u_fog_end, 1.0, 0.0);
  color = mix(color, environment_fog_color(), fog_amount);

  float core_alpha =
      (1.0 -
       smoothstep(0.045 + mud_weight * 0.025, 0.14 + mud_weight * 0.045, shore_t)) *
      smoothstep(0.40, 0.67, exposed_bank + transition_coverage * 0.12);
  float transition_envelope = 1.0 - smoothstep(0.12, shore_extent, shore_t);
  float broken_transition =
      transition_envelope * transition_coverage * (0.30 + exposed_bank * 0.70);
  float pebble_islands = (1.0 - smoothstep(0.26, shore_extent + 0.04, shore_t)) *
                         pebbles * (0.38 + stone_weight * 0.52);
  float grass_islands =
      (1.0 - smoothstep(0.30, shore_extent, shore_t)) * grass_cover * 0.72;
  float edge_fade = saturate(
      max(core_alpha, max(broken_transition, max(pebble_islands, grass_islands))));

  color += color * local_lighting(world_pos, normalize(v_normal));
  color = apply_directional_shadow(color, world_pos, v_normal);
  frag_color = vec4(saturate(color), segment_visibility * edge_fade);
}
