float soi_hash12_dbdbc1(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

float soi_noise2(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  float a = soi_hash12_dbdbc1(i);
  float b = soi_hash12_dbdbc1(i + vec2(1.0, 0.0));
  float c = soi_hash12_dbdbc1(i + vec2(0.0, 1.0));
  float d = soi_hash12_dbdbc1(i + vec2(1.0, 1.0));
  vec2 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float soi_fbm_23e5ab(vec2 p) {
  float value = 0.0;
  float amplitude = 0.5;
  for (int octave = 0; octave < 4; ++octave) {
    value += amplitude * soi_noise2(p);
    p = p * 2.03 + vec2(13.1, 7.7);
    amplitude *= 0.5;
  }
  return value;
}

float soi_hash_f8bd2f(vec2 p) {
  return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453123);
}

float soi_value_noise_6c4de2(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  vec2 u = f * f * (3.0 - 2.0 * f);

  float a = soi_hash_f8bd2f(i);
  float b = soi_hash_f8bd2f(i + vec2(1.0, 0.0));
  float c = soi_hash_f8bd2f(i + vec2(0.0, 1.0));
  float d = soi_hash_f8bd2f(i + vec2(1.0, 1.0));

  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float soi_fbm_d6cc9d(vec2 p, int octaves) {
  float value = 0.0;
  float amplitude = 0.5;
  float frequency = 1.0;

  for (int i = 0; i < octaves; i++) {
    value += amplitude * soi_value_noise_6c4de2(p * frequency);
    amplitude *= 0.5;
    frequency *= 2.0;
  }

  return value;
}

float soi_hash12_9f6e8e(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float soi_hash13_1c8396(vec3 p) {
  p = fract(p * 0.1031);
  p += dot(p, p.yzx + 33.33);
  return fract((p.x + p.y) * p.z);
}

float soi_hash13_a1b3c9(vec3 p) {
  return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

float soi_hash21_8b0317(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float soi_hash21_d64971(vec2 point) {
  point = fract(point * vec2(123.34, 456.21));
  point += dot(point, point + 45.32);
  return fract(point.x * point.y);
}

float soi_hash_15a407(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float soi_hash_565aa4(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float soi_hash_82bbee(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

float soi_noise21_b0e82b(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  return mix(
      mix(soi_hash12_9f6e8e(i), soi_hash12_9f6e8e(i + vec2(1.0, 0.0)), f.x),
      mix(soi_hash12_9f6e8e(i + vec2(0.0, 1.0)), soi_hash12_9f6e8e(i + vec2(1.0)), f.x),
      f.y);
}

float soi_noise21_cdf702(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  float a = soi_hash21_8b0317(i), b = soi_hash21_8b0317(i + vec2(1, 0)),
        c = soi_hash21_8b0317(i + vec2(0, 1)), d = soi_hash21_8b0317(i + vec2(1, 1));
  vec2 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float soi_noise3(vec3 p) {
  vec3 i = floor(p);
  vec3 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);

  float n000 = soi_hash13_1c8396(i + vec3(0.0, 0.0, 0.0));
  float n100 = soi_hash13_1c8396(i + vec3(1.0, 0.0, 0.0));
  float n010 = soi_hash13_1c8396(i + vec3(0.0, 1.0, 0.0));
  float n110 = soi_hash13_1c8396(i + vec3(1.0, 1.0, 0.0));
  float n001 = soi_hash13_1c8396(i + vec3(0.0, 0.0, 1.0));
  float n101 = soi_hash13_1c8396(i + vec3(1.0, 0.0, 1.0));
  float n011 = soi_hash13_1c8396(i + vec3(0.0, 1.0, 1.0));
  float n111 = soi_hash13_1c8396(i + vec3(1.0, 1.0, 1.0));

  float nx00 = mix(n000, n100, f.x);
  float nx10 = mix(n010, n110, f.x);
  float nx01 = mix(n001, n101, f.x);
  float nx11 = mix(n011, n111, f.x);

  float nxy0 = mix(nx00, nx10, f.y);
  float nxy1 = mix(nx01, nx11, f.y);

  return mix(nxy0, nxy1, f.z);
}

float soi_noise_3d41e6(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float a = soi_hash_82bbee(i);
  float b = soi_hash_82bbee(i + vec2(1.0, 0.0));
  float c = soi_hash_82bbee(i + vec2(0.0, 1.0));
  float d = soi_hash_82bbee(i + vec2(1.0, 1.0));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float soi_noise_95f501(vec2 p) {
  vec2 cell = floor(p);
  vec2 local = fract(p);
  vec2 smooth_local = local * local * (3.0 - 2.0 * local);

  float a = soi_hash_565aa4(cell);
  float b = soi_hash_565aa4(cell + vec2(1.0, 0.0));
  float c = soi_hash_565aa4(cell + vec2(0.0, 1.0));
  float d = soi_hash_565aa4(cell + vec2(1.0, 1.0));

  return mix(mix(a, b, smooth_local.x), mix(c, d, smooth_local.x), smooth_local.y);
}

float soi_value_noise_e2c097(vec2 point) {
  vec2 cell = floor(point);
  vec2 local = fract(point);
  local = local * local * local * (local * (local * 6.0 - 15.0) + 10.0);
  float a = soi_hash21_d64971(cell);
  float b = soi_hash21_d64971(cell + vec2(1.0, 0.0));
  float c = soi_hash21_d64971(cell + vec2(0.0, 1.0));
  float d = soi_hash21_d64971(cell + vec2(1.0, 1.0));
  return mix(mix(a, b, local.x), mix(c, d, local.x), local.y);
}
