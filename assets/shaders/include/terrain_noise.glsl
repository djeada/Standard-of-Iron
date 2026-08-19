float soi_hash21_8b0317(vec2 p);

vec2 soi_terrain_hash22_5a91c4(vec2 p) {
  vec2 h = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
  return -1.0 + 2.0 * fract(sin(h) * 43758.5453123);
}

float soi_terrain_value_noise_2f7ce8(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  float a = soi_hash21_8b0317(i);
  float b = soi_hash21_8b0317(i + vec2(1.0, 0.0));
  float c = soi_hash21_8b0317(i + vec2(0.0, 1.0));
  float d = soi_hash21_8b0317(i + vec2(1.0, 1.0));
  vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float soi_terrain_gradient_noise_d3b016(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
  float a = dot(soi_terrain_hash22_5a91c4(i), f);
  float b = dot(soi_terrain_hash22_5a91c4(i + vec2(1.0, 0.0)), f - vec2(1.0, 0.0));
  float c = dot(soi_terrain_hash22_5a91c4(i + vec2(0.0, 1.0)), f - vec2(0.0, 1.0));
  float d = dot(soi_terrain_hash22_5a91c4(i + vec2(1.0, 1.0)), f - vec2(1.0, 1.0));
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y) * 1.55;
}

float soi_terrain_gradient_fbm_7c25da(vec2 p, int octaves, float footprint) {
  float value = 0.0;
  float amplitude = 0.54;
  mat2 octave_rotation = mat2(0.80, -0.60, 0.60, 0.80);
  int count = clamp(octaves, 1, 5);
  for (int i = 0; i < count; ++i) {
    float fade = 1.0 - smoothstep(0.30, 0.85, footprint);
    if (fade <= 0.001) {
      break;
    }
    value += soi_terrain_gradient_noise_d3b016(p) * amplitude * fade;
    p = octave_rotation * p * 2.03 + vec2(7.1, -3.8);
    amplitude *= 0.49;
    footprint *= 2.03;
  }
  return value;
}

vec2 soi_terrain_cellular_distances_9e14b7(vec2 p) {
  vec2 cell = floor(p);
  vec2 local = fract(p);
  float nearest = 8.0;
  float second = 8.0;
  for (int y = -1; y <= 1; ++y) {
    for (int x = -1; x <= 1; ++x) {
      vec2 offset = vec2(float(x), float(y));
      vec2 point = 0.5 + 0.46 * soi_terrain_hash22_5a91c4(cell + offset);
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
