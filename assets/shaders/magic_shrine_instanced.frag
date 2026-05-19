#version 330 core

in vec3 v_world_pos;
in vec3 v_normal;
in vec3 v_color;
in vec3 v_local_pos;
flat in float v_seed;

uniform vec3 u_light_direction;
uniform vec3 u_camera_pos;
uniform float u_time;
uniform float u_magic_strength;

out vec4 frag_color;

float hash13(vec3 p) {
  p = fract(p * 0.1031);
  p += dot(p, p.yzx + 33.33);
  return fract((p.x + p.y) * p.z);
}

float noise3(vec3 p) {
  vec3 i = floor(p);
  vec3 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);

  float n000 = hash13(i + vec3(0.0, 0.0, 0.0));
  float n100 = hash13(i + vec3(1.0, 0.0, 0.0));
  float n010 = hash13(i + vec3(0.0, 1.0, 0.0));
  float n110 = hash13(i + vec3(1.0, 1.0, 0.0));
  float n001 = hash13(i + vec3(0.0, 0.0, 1.0));
  float n101 = hash13(i + vec3(1.0, 0.0, 1.0));
  float n011 = hash13(i + vec3(0.0, 1.0, 1.0));
  float n111 = hash13(i + vec3(1.0, 1.0, 1.0));

  float nx00 = mix(n000, n100, f.x);
  float nx10 = mix(n010, n110, f.x);
  float nx01 = mix(n001, n101, f.x);
  float nx11 = mix(n011, n111, f.x);

  float nxy0 = mix(nx00, nx10, f.y);
  float nxy1 = mix(nx01, nx11, f.y);

  return mix(nxy0, nxy1, f.z);
}

float fbm(vec3 p) {
  float v = 0.0;
  float a = 0.5;
  for (int i = 0; i < 4; i++) {
    v += noise3(p) * a;
    p = p * 2.03 + vec3(17.13, 7.91, 11.47);
    a *= 0.5;
  }
  return v;
}

void main() {
  vec3 N = normalize(v_normal);
  vec3 L = normalize(u_light_direction);
  vec3 V = normalize(u_camera_pos - v_world_pos);
  vec3 H = normalize(L + V);

  vec3 p = v_local_pos * 1.2;

  // Base stone texture
  float stone_large = fbm(p * 1.8);
  float stone_grain = fbm(p * 8.0);

  // Stone color — purple-tinged dark stone
  vec3 dark_stone = v_color * vec3(0.55, 0.50, 0.65);
  vec3 mid_stone  = v_color * vec3(0.80, 0.76, 0.92);
  vec3 stone_color = mix(dark_stone, mid_stone, stone_large);
  stone_color *= mix(0.78, 1.12, stone_grain);

  // Mortar lines on column / wall faces
  float mortar_h = 1.0 - smoothstep(0.03, 0.13, fract(v_local_pos.y * 2.4));
  float mortar_v = 1.0 - smoothstep(0.03, 0.13,
      fract(v_local_pos.x * 1.7 + v_local_pos.z * 1.3));
  stone_color *= 1.0 - max(mortar_h, mortar_v) * 0.18;

  // Arcane rune pattern etched on surfaces
  float rune_u = fract(v_local_pos.x * 3.1 + v_local_pos.z * 2.7);
  float rune_v = fract(v_local_pos.y * 4.5 + v_local_pos.z * 1.9);
  float rune_cross = max(
      1.0 - smoothstep(0.0, 0.07, abs(rune_u - 0.5)),
      1.0 - smoothstep(0.0, 0.07, abs(rune_v - 0.5))
  );
  float rune_h = hash13(floor(v_local_pos * vec3(3.1, 4.5, 2.7)) + vec3(v_seed * 7.0));
  float rune = rune_cross * step(0.55, rune_h);

  // Magic glow colors — violet and cyan
  vec3 magic_a = vec3(0.55, 0.10, 1.30);  // violet
  vec3 magic_b = vec3(0.10, 0.70, 1.40);  // cyan

  float magic_blend = fbm(p * 1.4 + vec3(v_seed * 4.2));
  vec3 magic_color = mix(magic_a, magic_b, magic_blend);

  // Pulsing glow on rune lines
  float pulse = 0.72 + 0.28 * sin(u_time * 1.8 + v_seed * 6.28318 +
      fbm(p * 1.5 + vec3(u_time * 0.3)) * 4.0);

  float magic_strength = max(u_magic_strength, 0.0);

  // Lighting
  vec3 sun_color = vec3(1.05, 0.90, 0.75);
  vec3 sky_color = vec3(0.60, 0.65, 1.00);

  float ndotl = max(dot(N, L), 0.0);
  float hemi = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
  float ao = clamp(N.y * 0.45 + 0.72, 0.28, 1.0);

  vec3 ambient = sky_color * (0.18 + hemi * 0.16);
  vec3 direct  = sun_color * ndotl * 0.78;

  float spec_base = max(dot(N, H), 0.0);
  float specular  = pow(spec_base, 36.0) * 0.12;

  float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);

  // Rim light from sky
  float rim = pow(1.0 - max(dot(N, V), 0.0), 3.5) * 0.12;
  vec3 rim_color = sky_color * rim;

  vec3 color = stone_color * (ambient + direct) * ao;
  color += sun_color * specular * ao;
  color += rim_color;

  // Magic glow
  vec3 glow = magic_color * magic_strength * pulse *
      (rune * 2.0 + fresnel * 0.35);
  color += glow;

  // Ambient arcane shimmer from altar bowl area (top surface)
  float top_mask = smoothstep(0.60, 0.90, v_local_pos.y / max(abs(v_local_pos.y) + 0.01, 1.0));
  float shimmer = top_mask * (0.5 + 0.5 * sin(u_time * 2.6 + v_seed * 9.3)) * magic_strength;
  color += magic_color * shimmer * 0.30;

  frag_color = vec4(color, 1.0);
}
