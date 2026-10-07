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

const float k_ore_crown = 0.27;

const vec3 k_vein_hot = vec3(1.90, 0.62, 0.30);
const vec3 k_vein_blood = vec3(0.92, 0.035, 0.050);
const vec3 k_vein_abyss = vec3(0.34, 0.025, 0.36);
const vec3 k_meteoric_black = vec3(0.050, 0.050, 0.060);
const vec3 k_meteoric_steel = vec3(0.30, 0.31, 0.35);
const vec3 k_crystal_black = vec3(0.020, 0.016, 0.026);
const vec3 k_rust = vec3(0.24, 0.085, 0.050);

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

float glow_line(float distance, float width) {
  float aa = max(fwidth(distance), 1.0e-4);
  float spread = max(width, aa * 1.5);
  return (1.0 - smoothstep(spread * 0.30, spread, distance)) * sqrt(width / spread);
}

void main() {
  vec3 N = normalize(v_normal);
  vec3 L = environment_primary_direction();
  vec3 V = normalize(u_camera_pos - v_world_pos);
  vec3 H = normalize(L + V);

  vec3 lp = v_local_pos;
  vec3 p = lp * 1.5;

  float warp_a = fbm(p * 1.15 + vec3(v_seed * 3.1));
  float warp_b = fbm(p.yzx * 1.40 + vec3(4.7, 1.3, 8.2) + v_seed);
  vec3 q = p + vec3(warp_a, warp_b, warp_a * 0.7) * 0.55;

  float stone_large = fbm(q * 2.4);
  float stone_grain = fbm(q * 11.0);
  float mineral_noise = fbm(q * 5.5 + vec3(6.0));

  float crown = k_ore_crown + 0.04 * (stone_large - 0.5);
  float crystal = smoothstep(crown, crown + 0.05, lp.y);
  float shard_height = clamp((lp.y - k_ore_crown) / 0.48, 0.0, 1.0);
  float facet_key =
      soi_hash13_1c8396(floor(N * 4.0 + vec3(0.5)) + vec3(v_seed * 7.0, 0.0, 0.0));

  float etch_a = abs(fract(dot(q, vec3(0.82, 0.30, 0.48)) * 9.0) - 0.5);
  float etch_b = abs(fract(dot(q, vec3(-0.40, 0.55, 0.73)) * 9.0) - 0.5);
  float etch_c = abs(fract(dot(q, vec3(0.20, -0.85, 0.48)) * 9.0) - 0.5);
  float etch = 1.0 - smoothstep(0.02, 0.07, min(etch_a, min(etch_b, etch_c)));
  float polished = smoothstep(0.52, 0.70, stone_large);

  vec3 rock_color = mix(k_meteoric_black, v_color * 0.40, stone_large * 0.6);
  rock_color *= mix(0.70, 1.12, stone_grain);
  rock_color = mix(rock_color, k_meteoric_steel, polished * (0.25 + 0.30 * etch));
  rock_color = mix(rock_color,
                   k_rust,
                   smoothstep(0.55, 0.80, mineral_noise) *
                       (1.0 - smoothstep(0.02, 0.18, lp.y)) * 0.55);

  float field1 = sin(q.y * 7.0 + q.x * 2.8 - q.z * 2.2 + fbm(q * 2.0) * 5.5);
  float field2 =
      sin(q.x * 8.5 + q.z * 4.2 + q.y * 1.8 + fbm(q * 2.7 + vec3(11.0)) * 4.0);
  float vein_core =
      max(glow_line(abs(field1), 0.10), glow_line(abs(field2), 0.075) * 0.85);
  float vein_halo =
      max(glow_line(abs(field1), 0.42), glow_line(abs(field2), 0.34) * 0.75);
  float vein_live = smoothstep(0.30, 0.55, fbm(q * 1.3 + vec3(v_seed * 4.0, 3.0, 1.0)));
  vein_core *= vein_live * (1.0 - crystal);
  vein_halo *= vein_live * (1.0 - crystal);

  rock_color *= 1.0 - vein_halo * 0.45;

  vec3 crystal_color = k_crystal_black * mix(0.7, 1.5, facet_key);
  vec3 albedo = mix(rock_color, crystal_color, crystal);

  float metal = max(polished * 0.6 * (1.0 - crystal), crystal);

  vec3 sun_color = environment_primary_color() * environment_primary_intensity();

  float ao = clamp(N.y * 0.45 + 0.68, 0.28, 1.0);
  vec3 illumination = soi_surface_lighting_scaled(N, 0.82);

  float n_dot_h = max(dot(N, H), 0.0);
  float rock_spec = pow(n_dot_h, mix(30.0, 70.0, polished)) * mix(0.06, 0.42, polished);
  float crystal_spec = pow(n_dot_h, 140.0) * 1.6 * (0.45 + 0.55 * facet_key);
  float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);

  vec3 color = albedo * illumination * ao;
  color += sun_color * vec3(0.80, 0.84, 0.96) * rock_spec * ao * (1.0 - crystal);
  color += sun_color * vec3(0.92, 0.90, 1.00) * crystal_spec * crystal;
  color += soi_rim_light(N, V) * mix(0.6, 1.3, metal);
  color += environment_sky_color() * fresnel * crystal * 0.18;

  color = apply_directional_shadow(color, v_world_pos, v_normal);

  float strength = max(u_magic_strength, 0.0);
  float night = environment_night_amount();
  float kindle = mix(0.85, 1.25, night);

  float throb_phase = fract(u_time * 0.45 + v_seed + lp.y * 0.6);
  float throb = exp(-pow((throb_phase - 0.10) * 9.0, 2.0)) +
                0.55 * exp(-pow((throb_phase - 0.28) * 9.0, 2.0));
  float flow = fbm(q * 1.8 + vec3(0.0, -u_time * 0.30, u_time * 0.07));
  float pulse = (0.66 + 0.34 * throb) * (0.70 + 0.30 * smoothstep(0.3, 0.7, flow));

  vec3 vein_color = mix(k_vein_blood, k_vein_hot, smoothstep(0.55, 1.0, vein_core));
  vec3 emission = vein_color * vein_core * 1.25;
  emission += k_vein_abyss * vein_halo * (1.0 - vein_core) * (0.08 + 0.14 * night);

  float root_glow = crystal * (1.0 - smoothstep(0.0, 0.28, shard_height));
  float edge_glow = crystal * pow(fresnel, 1.4) * (0.30 + 0.70 * (1.0 - shard_height));
  float lit_facet = crystal * step(0.86, facet_key) * (1.0 - shard_height * 0.7);
  float seam_field = sin(q.x * 13.0 + q.z * 11.0 - q.y * 3.0 + fbm(q * 3.1) * 4.5);
  float crystal_seam = crystal * glow_line(abs(seam_field), 0.09) *
                       (1.0 - smoothstep(0.35, 0.95, shard_height));
  emission += mix(k_vein_blood, k_vein_hot, 0.30) * root_glow * 0.95;
  emission += mix(k_vein_blood, k_vein_hot, 0.45) * crystal_seam * 1.20;
  emission += mix(k_vein_abyss, k_vein_blood, 0.70) * edge_glow * 0.60;
  emission += k_vein_blood * lit_facet * 0.22;

  color += emission * pulse * strength * kindle;
  color += albedo * ao * local_lighting(v_world_pos, normalize(v_normal));
  color = apply_visibility_world_shading(color, v_world_pos.xz);
  frag_color = vec4(color, 1.0);
}
