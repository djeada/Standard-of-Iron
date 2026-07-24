#version 330 core

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
uniform vec3 u_camera_pos;
uniform vec3 u_light_dir;

uniform vec3 u_fog_color;
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

  float wash_speed = mix(0.025, 0.065, river);

  vec2 world_uv = world_pos.xz * 0.22;

  vec2 slow_wash = vec2(time * wash_speed, -time * wash_speed * 0.45);

  float macro = fbm(world_uv * 0.42 + vec2(11.0, -7.0));

  float detail = fbm(world_uv * 1.65 + vec2(-3.0, 17.0));

  float grain = value_noise(world_uv * 8.5 + vec2(29.0));

  float wet_edge_noise = (fbm(world_uv * 0.85 + slow_wash) - 0.5) * 0.045;

  float shore_t = saturate(tex_coord.x + wet_edge_noise);

  float wetness = 1.0 - smoothstep(0.055, 0.62, shore_t);

  float contact = 1.0 - smoothstep(0.008, 0.105, shore_t);

  float damp_band =
      smoothstep(0.03, 0.30, shore_t) * (1.0 - smoothstep(0.34, 0.72, shore_t));

  float moisture = saturate(u_moisture_level);
  float rock_exposure = saturate(u_rock_exposure);
  float snow_coverage = saturate(u_snow_coverage);

  vec3 grass_primary = max(u_ground_color, vec3(0.025));
  vec3 grass_secondary = max(u_grass_secondary, vec3(0.025));
  vec3 grass_dry = max(u_grass_dry, vec3(0.025));
  vec3 soil = max(u_soil_color, vec3(0.025));
  vec3 rock_low = max(u_rock_low, vec3(0.025));
  vec3 rock_high = max(u_rock_high, vec3(0.025));

  vec3 terrain_grass =
      mix(grass_primary, grass_secondary, smoothstep(0.24, 0.76, macro));
  float terrain_dryness = saturate((1.0 - moisture) * 0.42 + (detail - 0.42) * 0.24);
  terrain_grass = mix(terrain_grass, grass_dry, terrain_dryness);

  float terrain_soil_mask =
      smoothstep(0.62, 0.84, macro * 0.58 + detail * 0.42 + moisture * 0.07);
  vec3 terrain_soil = mix(soil, grass_dry, 0.14 + (1.0 - moisture) * 0.12);

  float terrain_rock_mask =
      smoothstep(0.65, 0.88, detail + rock_exposure * 0.18) * rock_exposure;
  vec3 terrain_rock = mix(rock_low, rock_high, detail);

  vec3 map_ground = mix(terrain_grass, terrain_soil, terrain_soil_mask * 0.42);
  map_ground = mix(map_ground, terrain_rock, terrain_rock_mask * 0.52);

  float bank_snow = smoothstep(0.54, 0.78, macro + detail * 0.12) * snow_coverage;
  map_ground = mix(map_ground, max(u_snow_color, vec3(0.025)), bank_snow * 0.82);

  vec3 wet_silt = soil * mix(vec3(0.60, 0.62, 0.57), vec3(0.50, 0.55, 0.50), moisture);

  vec3 damp_earth =
      soil * mix(vec3(0.78, 0.80, 0.72), vec3(0.68, 0.73, 0.66), moisture);

  vec3 dry_earth = mix(soil, grass_dry, 0.20) * mix(0.92, 0.84, moisture);

  vec3 bank_grass = terrain_grass;

  vec3 biome_stone = mix(rock_low, rock_high, grain * 0.62 + detail * 0.38);

  vec3 earth = mix(wet_silt, damp_earth, smoothstep(0.03, 0.46, shore_t));

  earth = mix(earth, dry_earth, smoothstep(0.52, 0.90, shore_t));

  float earth_variation =
      0.83 + macro * 0.10 + (detail - 0.5) * 0.055 + (grain - 0.5) * 0.025;

  earth *= earth_variation;

  float land_merge = smoothstep(0.72, 0.96, shore_t);

  float grass_distribution = smoothstep(0.31, 0.72, macro + detail * 0.18);

  float grass_cover = smoothstep(0.50, 0.88, shore_t) *
                      mix(0.24, 0.72, grass_distribution) *
                      (1.0 - rock_exposure * 0.36);

  vec3 grass_color = bank_grass * mix(0.86, 0.98, detail);

  earth = mix(earth, grass_color, grass_cover);

  earth = mix(earth, map_ground * mix(0.90, 0.98, detail), land_merge);

  float pebble_cells = value_noise(world_uv * 4.6 + vec2(41.0, -13.0));

  float pebble_breakup = value_noise(world_uv * 12.0 + vec2(-19.0, 8.0));

  float pebble_region =
      smoothstep(0.11, 0.42, shore_t) * (1.0 - smoothstep(0.58, 0.76, shore_t));

  float pebbles = smoothstep(0.79, 0.94, pebble_cells) *
                  smoothstep(0.54, 0.82, pebble_breakup) * pebble_region;

  vec3 stone_color = biome_stone * mix(0.78, 0.94, grain);

  earth = mix(earth, stone_color, pebbles * mix(0.24, 0.52, rock_exposure));

  float deposited_silt_noise =
      fbm(vec2(tex_coord.y * 7.0 + macro, world_uv.y * 0.35 - slow_wash.x));

  float deposited_silt = damp_band * smoothstep(0.62, 0.86, deposited_silt_noise);

  earth *= 1.0 - deposited_silt * 0.11;

  vec3 base_normal = safe_normalize(v_normal, vec3(0.0, 1.0, 0.0));

  vec3 normal =
      procedural_detail_normal(base_normal, world_uv * 2.8, 0.42 * (0.35 + grain));

  vec3 light_dir = safe_normalize(u_light_dir, vec3(0.0, 1.0, 0.0));

  vec3 view_dir = safe_normalize(u_camera_pos - world_pos, normal);

  float ndl = max(dot(normal, light_dir), 0.0);

  float ambient_occlusion = mix(0.78, 0.94, shore_t);

  float diffuse_light = 0.56 * ambient_occlusion + ndl * 0.38;

  vec3 color = earth * diffuse_light;

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

  float safe_fog_end = max(u_fog_start + 0.001, u_fog_end);

  float fog_amount = smoothstep(u_fog_start, safe_fog_end, view_distance);

  color = mix(color, u_fog_color, fog_amount);

  frag_color = vec4(saturate(color), segment_visibility);
}
