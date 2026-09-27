#version 330 core
#include "directional_shadows.glsl"
#include "environment_lighting.glsl"
#include "local_lighting.glsl"
#include "noise.glsl"
#include "visibility_mask.glsl"

in vec3 v_world_pos;
in vec3 v_normal;
in vec3 v_color;
in vec3 v_local_pos;
in vec3 v_local_normal;
flat in float v_seed;

uniform vec3 u_camera_pos;
uniform float u_time;
uniform float u_magic_strength;

out vec4 frag_color;

float fbm(vec3 p) {
  float v = 0.0;
  float a = 0.5;
  for (int i = 0; i < 4; i++) {
    v += soi_noise3(p) * a;
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

float stroke(vec2 p, vec2 a, vec2 b, float width) {
  vec2 ab = b - a;
  float t = clamp(dot(p - a, ab) / dot(ab, ab), 0.0, 1.0);
  float distance = length(p - a - ab * t);
  float aa = max(fwidth(distance), 0.003);
  return 1.0 - smoothstep(width, width + aa, distance);
}

float inscription(vec2 uv, float seed) {
  vec2 cell = floor(uv);
  vec2 p = fract(uv);
  float choice = soi_hash13_1c8396(vec3(cell, seed * 11.0));
  float stem = stroke(p, vec2(0.5, 0.18), vec2(0.5, 0.82), 0.022);
  float upper = stroke(p, vec2(0.5, 0.76), vec2(0.77, 0.57), 0.022);
  float lower = stroke(p, vec2(0.5, 0.45), vec2(0.23, 0.65), 0.022);
  float foot = stroke(p, vec2(0.5, 0.20), vec2(0.76, 0.39), 0.022);
  return max(stem,
             max(upper, max(lower * step(0.35, choice), foot * step(0.65, choice)))) *
         step(0.38, choice);
}

vec3 relief_normal(vec3 n, float height) {
  vec3 dx = dFdx(v_world_pos);
  vec3 dy = dFdy(v_world_pos);
  vec3 a = cross(dy, n);
  vec3 b = cross(n, dx);
  float determinant = dot(dx, a);
  vec3 gradient = sign(determinant) * (dFdx(height) * a + dFdy(height) * b);
  if (abs(determinant) < 1.0e-12) {
    return n;
  }
  return normalize(abs(determinant) * n - gradient);
}

void main() {
  vec3 N = normalize(v_normal);
  vec3 L = environment_primary_direction();
  vec3 V = normalize(u_camera_pos - v_world_pos);
  vec3 H = normalize(L + V);

  vec3 p = v_local_pos * 1.12;

  float stone_large = fbm(p * 1.6 + vec3(3.0, 1.0, 6.0));
  float stone_grain = fbm(p * 7.4 + vec3(8.0, 2.0, 4.0));
  float weathering = fbm(p * 12.0 + vec3(v_seed * 7.0));

  vec3 basalt = v_color * vec3(0.30, 0.27, 0.42);
  vec3 moonstone = v_color * vec3(0.72, 0.62, 0.94);
  vec3 ash = vec3(0.21, 0.16, 0.27);
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

  vec2 rune_uv = project_rune_coords(v_local_pos, normalize(v_local_normal));
  float rune = inscription(rune_uv * vec2(7.0, 5.5), v_seed);
  float top_face = smoothstep(0.55, 0.85, v_local_normal.y);
  float radial = length(v_local_pos.xz);
  float ring_distance = min(abs(radial - 0.20), abs(radial - 0.145));
  float ring_aa = max(fwidth(radial), 0.0015);
  float sigil = 1.0 - smoothstep(0.004, 0.004 + ring_aa, ring_distance);
  float spokes = max(stroke(v_local_pos.xz, vec2(-0.20, 0.0), vec2(0.20, 0.0), 0.002),
                     stroke(v_local_pos.xz, vec2(0.0, -0.20), vec2(0.0, 0.20), 0.002));
  sigil = max(sigil, spokes * (1.0 - smoothstep(0.13, 0.19, radial)));
  float altar_top = top_face * smoothstep(0.78, 0.92, v_local_pos.y);
  rune = mix(rune, sigil, altar_top);
  float vein_field = fbm(p * 3.8 + vec3(v_seed));
  float veins = 1.0 - smoothstep(0.008, 0.025, abs(vein_field - 0.48));
  stone_color = mix(stone_color, moonstone * 1.12, veins * 0.22);
  stone_color *= 1.0 - rune * 0.48;
  stone_color *= mix(0.68, 1.0, smoothstep(0.02, 0.32, v_local_pos.y));
  float grain_detail = 1.0 - smoothstep(0.04, 0.16, length(fwidth(p)));
  N = relief_normal(N, (stone_grain * 0.003 - rune * 0.0025) * grain_detail);

  vec3 magic_a = vec3(0.48, 0.20, 0.86);
  vec3 magic_b = vec3(0.18, 0.52, 0.82);
  vec3 sanctum_gold = vec3(1.02, 0.54, 0.18);

  float magic_blend = fbm(p * 1.4 + vec3(v_seed * 4.2, 5.0, 1.0));
  vec3 magic_color = mix(magic_a, magic_b, magic_blend);
  vec3 sanctum_color = mix(magic_color, sanctum_gold, 0.34);

  float breathing = 0.5 + 0.5 * sin(u_time * 1.25 + v_seed * 6.28318);
  float procession =
      0.5 + 0.5 * sin(v_local_pos.y * 8.0 - u_time * 1.8 + v_seed * 6.28318);
  float pulse = 0.62 + 0.24 * breathing + 0.14 * pow(procession, 4.0);

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

  vec3 sun_color = environment_primary_color() * environment_primary_intensity();
  vec3 sky_color = environment_sky_color();

  float ndotl = max(dot(N, L), 0.0);
  float hemi = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
  float ao = clamp(N.y * 0.45 + 0.72, 0.28, 1.0);

  vec3 ambient = environment_ambient_light(N);
  vec3 direct = soi_key_light(N) * 0.78;

  float spec_base = max(dot(N, H), 0.0);
  float specular = pow(spec_base, 36.0) * 0.12;

  float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);

  float rim = pow(1.0 - max(dot(N, V), 0.0), 3.5) * 0.12;
  vec3 rim_color = sky_color * rim;

  vec3 color = stone_color * (ambient + direct) * ao * environment_exposure();
  color += soi_rim_light(N, V);
  color += sun_color * specular * ao;
  color += rim_color;

  vec3 glow = magic_color * magic_strength * pulse *
              (rune * 1.15 + obelisk_mask * (0.10 + veins * 0.16) + fresnel * 0.045);
  vec3 altar_glow =
      sanctum_color * magic_strength *
      (altar_core * (0.78 + 0.22 * pulse) + altar_ring * (0.50 + 0.18 * pulse));
  vec3 aura = mix(magic_color, sanctum_color, 0.45) * magic_strength * altar_aura *
              (0.18 + 0.12 * pulse);
  color = apply_directional_shadow(color, v_world_pos, v_normal);
  color += glow;
  color += altar_glow;
  color += aura;

  color += stone_color * ao * local_lighting(v_world_pos, normalize(v_normal));
  color = apply_visibility_world_shading(color, v_world_pos.xz);
  frag_color = vec4(color, 1.0);
}
