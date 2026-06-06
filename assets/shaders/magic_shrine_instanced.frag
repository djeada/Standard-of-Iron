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

vec2 project_rune_coords(vec3 p, vec3 n) {
  vec3 an = abs(n);
  if (an.y > an.x && an.y > an.z) {
    return p.xz;
  }
  if (an.x > an.z) {
    return p.zy;
  }
  return p.xy;
}

void main() {
  vec3 N = normalize(v_normal);
  vec3 L = normalize(u_light_direction);
  vec3 V = normalize(u_camera_pos - v_world_pos);
  vec3 H = normalize(L + V);

  vec3 p = v_local_pos * 1.12;

  float stone_large = fbm(p * 1.6 + vec3(3.0, 1.0, 6.0));
  float stone_grain = fbm(p * 7.4 + vec3(8.0, 2.0, 4.0));
  float weathering = fbm(p * 12.0 + vec3(v_seed * 7.0));

  vec3 basalt = v_color * vec3(0.44, 0.42, 0.56);
  vec3 moonstone = v_color * vec3(0.84, 0.82, 1.02);
  vec3 ash = vec3(0.36, 0.32, 0.44);
  vec3 stone_color = mix(basalt, moonstone, stone_large);
  stone_color *= mix(0.72, 1.10, stone_grain);
  stone_color = mix(stone_color, ash, weathering * 0.18);

  float block_bands = 1.0 - smoothstep(0.03, 0.11, fract(v_local_pos.y * 2.6));
  float side_face = 1.0 - abs(N.y);
  float seam_x =
      1.0 - smoothstep(0.0, 0.06, abs(fract(v_local_pos.x * 1.55 + 0.5) - 0.5));
  float seam_z =
      1.0 - smoothstep(0.0, 0.06, abs(fract(v_local_pos.z * 1.55 + 0.5) - 0.5));
  float seam = max(seam_x, seam_z) * side_face * smoothstep(0.12, 1.18, v_local_pos.y);
  stone_color *= 1.0 - max(block_bands * 0.16, seam * 0.08);

  vec2 rune_uv = project_rune_coords(v_local_pos * 1.65, N);
  vec2 rune_scale = vec2(2.8, 4.1);
  vec2 rune_cell = floor(rune_uv * rune_scale);
  vec2 rune_frac = fract(rune_uv * rune_scale);
  float rune_line_x = 1.0 - smoothstep(0.0, 0.09, abs(rune_frac.x - 0.5));
  float rune_line_y = 1.0 - smoothstep(0.0, 0.09, abs(rune_frac.y - 0.5));
  float rune_diag = 1.0 - smoothstep(0.0, 0.07, abs((rune_frac.x + rune_frac.y) - 1.0));
  float rune_selector = hash13(vec3(rune_cell, v_seed * 11.0));
  float rune = max(max(rune_line_x, rune_line_y) * step(0.46, rune_selector),
                   rune_diag * step(0.78, rune_selector));

  vec3 magic_a = vec3(0.60, 0.12, 1.34);
  vec3 magic_b = vec3(0.08, 0.82, 1.42);
  vec3 sanctum_gold = vec3(1.14, 0.84, 0.48);

  float magic_blend = fbm(p * 1.4 + vec3(v_seed * 4.2, 5.0, 1.0));
  vec3 magic_color = mix(magic_a, magic_b, magic_blend);
  vec3 sanctum_color = mix(magic_color, sanctum_gold, 0.34);

  float pulse = 0.74 + 0.26 * sin(u_time * 1.9 + v_seed * 6.28318 +
                                  fbm(p * 1.7 + vec3(u_time * 0.28)) * 4.8);

  float magic_strength = max(u_magic_strength, 0.0);

  float altar_radius = length(v_local_pos.xz);
  float altar_core = (1.0 - smoothstep(0.04, 0.18, altar_radius)) *
                     smoothstep(0.80, 0.96, v_local_pos.y);
  float altar_ring = (1.0 - smoothstep(0.0, 0.05, abs(altar_radius - 0.20))) *
                     smoothstep(0.76, 0.94, v_local_pos.y);
  float altar_aura = (1.0 - smoothstep(0.18, 0.42, altar_radius)) *
                     smoothstep(0.68, 1.02, v_local_pos.y);

  float obelisk_dist = min(min(length(v_local_pos.xz - vec2(-0.54, -0.54)),
                               length(v_local_pos.xz - vec2(0.54, -0.54))),
                           min(length(v_local_pos.xz - vec2(-0.54, 0.54)),
                               length(v_local_pos.xz - vec2(0.54, 0.54))));
  float obelisk_mask = (1.0 - smoothstep(0.02, 0.18, obelisk_dist)) *
                       smoothstep(0.26, 1.22, v_local_pos.y);

  vec3 sun_color = vec3(1.05, 0.90, 0.75);
  vec3 sky_color = vec3(0.60, 0.65, 1.00);

  float ndotl = max(dot(N, L), 0.0);
  float hemi = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
  float ao = clamp(N.y * 0.45 + 0.72, 0.28, 1.0);

  vec3 ambient = sky_color * (0.18 + hemi * 0.16);
  vec3 direct = sun_color * ndotl * 0.78;

  float spec_base = max(dot(N, H), 0.0);
  float specular = pow(spec_base, 36.0) * 0.12;

  float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);

  float rim = pow(1.0 - max(dot(N, V), 0.0), 3.5) * 0.12;
  vec3 rim_color = sky_color * rim;

  vec3 color = stone_color * (ambient + direct) * ao;
  color += sun_color * specular * ao;
  color += rim_color;

  vec3 glow = magic_color * magic_strength * pulse *
              (rune * 1.7 + seam * 0.65 + obelisk_mask * 0.22 + fresnel * 0.38);
  vec3 altar_glow =
      sanctum_color * magic_strength *
      (altar_core * (1.55 + 0.45 * pulse) + altar_ring * (0.95 + 0.30 * pulse));
  vec3 aura = mix(magic_color, sanctum_color, 0.45) * magic_strength * altar_aura *
              (0.26 + 0.18 * pulse);
  color += glow;
  color += altar_glow;
  color += aura;

  frag_color = vec4(color, 1.0);
}
