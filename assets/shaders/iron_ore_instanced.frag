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
flat in float v_seed;

uniform vec3 u_camera_pos;
uniform float u_time;
uniform float u_magic_strength;

out vec4 frag_color;

vec3 hash33(vec3 p) {
  p = fract(p * vec3(0.1031, 0.11369, 0.13787));
  p += dot(p, p.yxz + 19.19);
  return fract(vec3((p.x + p.y) * p.z, (p.x + p.z) * p.y, (p.y + p.z) * p.x));
}

float fbm(vec3 p) {
  float v = 0.0;
  float a = 0.5;

  for (int i = 0; i < 5; i++) {
    v += soi_noise3(p) * a;
    p = p * 2.03 + vec3(17.13, 7.91, 11.47);
    a *= 0.5;
  }

  return v;
}

float vein_line(float x, float width) {
  return 1.0 - smoothstep(0.0, width, abs(x));
}

void main() {
  vec3 N = normalize(v_normal);
  vec3 L = environment_primary_direction();
  vec3 V = normalize(u_camera_pos - v_world_pos);
  vec3 H = normalize(L + V);

  vec3 p = v_local_pos * 1.35;

  float warp_a = fbm(p * 1.15 + vec3(v_seed * 3.1));
  float warp_b = fbm(p.yzx * 1.40 + vec3(4.7, 1.3, 8.2) + v_seed);
  float warp_c = fbm(p.zxy * 1.05 + vec3(9.1, 5.4, 2.8) + v_seed * 2.0);

  vec3 q = p + vec3(warp_a, warp_b, warp_c) * 0.55;

  float stone_large = fbm(q * 2.4);
  float stone_grain = fbm(q * 11.0);
  float mineral_noise = fbm(q * 5.5 + vec3(6.0));

  float field1 = sin(q.y * 7.0 + q.x * 2.8 - q.z * 2.2 + fbm(q * 2.0) * 5.5);

  float field2 =
      sin(q.x * 8.5 + q.z * 4.2 + q.y * 1.8 + fbm(q * 2.7 + vec3(11.0)) * 4.0);

  float wide1 = vein_line(field1, 0.42);
  float wide2 = vein_line(field2, 0.34) * 0.75;

  float core1 = vein_line(field1, 0.075);
  float core2 = vein_line(field2, 0.055) * 0.85;

  float vein_wide = clamp(max(wide1, wide2), 0.0, 1.0);
  float vein_core = clamp(max(core1, core2), 0.0, 1.0);

  vec3 deep_rock = v_color * vec3(0.46, 0.48, 0.55);
  vec3 cool_rock = v_color * vec3(0.70, 0.72, 0.82);
  vec3 warm_mineral = vec3(0.48, 0.30, 0.20);

  vec3 rock_color = mix(deep_rock, cool_rock, stone_large);
  rock_color *= mix(0.72, 1.08, stone_grain);
  rock_color = mix(rock_color, warm_mineral, mineral_noise * 0.18);

  vec3 magic_a = vec3(0.14, 0.85, 1.35);
  vec3 magic_b = vec3(0.82, 0.24, 1.45);
  vec3 magic_color = mix(magic_a, magic_b, fbm(q * 1.7 + vec3(v_seed)));

  vec3 stained_ore = mix(rock_color, magic_color * 0.55, vein_wide * 0.45);
  vec3 albedo = stained_ore;

  vec3 cell_p = q * 9.0;
  vec3 cell = floor(cell_p);
  vec3 cell_f = fract(cell_p);

  vec3 crystal_center = hash33(cell + vec3(v_seed * 13.0));
  float crystal_rand = soi_hash13_1c8396(cell + vec3(31.7, 9.2, v_seed));

  float crystal_dist = length(cell_f - crystal_center);
  float crystal_shape = 1.0 - smoothstep(0.0, 0.16, crystal_dist);
  float crystal_mask = crystal_shape * step(0.925, crystal_rand);

  float crystal_twinkle =
      0.65 + 0.35 * sin(u_time * 3.2 + crystal_rand * 18.0 + v_seed * 6.28318);

  float crystal = crystal_mask * crystal_twinkle * mix(0.35, 1.0, vein_wide);

  vec3 sun_color = environment_primary_color() * environment_primary_intensity();
  vec3 sky_color = environment_sky_color();

  float ndotl = max(dot(N, L), 0.0);
  float hemi = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);

  float ao = clamp(N.y * 0.45 + 0.68, 0.28, 1.0);

  vec3 ambient = environment_ambient_light(N);
  vec3 direct = sun_color * ndotl * 0.82;

  float spec_base = max(dot(N, H), 0.0);
  float ore_spec = pow(spec_base, 42.0) * (0.08 + vein_wide * 0.35);
  float crystal_spec = pow(spec_base, 96.0) * crystal * 1.5;

  float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);

  float pulse = 0.78 + 0.22 * sin(u_time * 2.1 + v_seed * 12.0 + fbm(q * 2.0) * 5.0);

  float magic_strength = max(u_magic_strength, 0.0);

  vec3 glow = magic_color * magic_strength * pulse *
              (vein_core * 1.75 + vein_wide * 0.22 + fresnel * vein_wide * 0.45 +
               crystal * 1.35);

  vec3 color = albedo * (ambient + direct) * ao * environment_exposure();
  color += sun_color * ore_spec * ao;
  color += magic_color * crystal_spec;
  color += glow;

  color += color * local_lighting(v_world_pos, normalize(v_normal));
  color = apply_directional_shadow(color, v_world_pos, v_normal);
  color = apply_visibility_memory(color, v_world_pos.xz);
  frag_color = vec4(color, 1.0);
}
