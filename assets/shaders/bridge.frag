#version 330 core
#include "directional_shadows.glsl"
#include "environment_lighting.glsl"
#include "fog_reveal.glsl"
#include "local_lighting.glsl"
#include "noise.glsl"
#include "visibility_mask.glsl"

in vec3 v_normal;
in vec2 v_tex_coord;
in vec3 v_world_pos;
flat in int v_dressed_stone;

uniform vec3 u_color;
uniform vec3 u_camera_pos;

out vec4 frag_color;

const float PI = 3.14159265359;

float fbm(vec2 p) {
  float value = 0.0;
  float amplitude = 0.5;
  for (int i = 0; i < 4; ++i) {
    value += amplitude * soi_noise_3d41e6(p);
    p = mat2(1.6, -1.2, 1.2, 1.6) * p + vec2(7.1, 3.8);
    amplitude *= 0.5;
  }
  return value;
}

vec3 relief_normal(vec3 normal, float height) {
  vec3 dpdx = dFdx(v_world_pos);
  vec3 dpdy = dFdy(v_world_pos);
  vec3 across_x = cross(dpdy, normal);
  vec3 across_y = cross(normal, dpdx);
  float determinant = dot(dpdx, across_x);
  vec3 gradient = (dFdx(height) * across_x + dFdy(height) * across_y) *
                  sign(determinant) / max(abs(determinant), 0.000001);
  gradient /= max(1.0, length(gradient) / 0.65);
  return normalize(normal - gradient);
}

float fresnel_schlick(float cos_theta, float F0) {
  return F0 + (1.0 - F0) * pow(1.0 - cos_theta, 5.0);
}

float ggx_specular(vec3 N, vec3 V, vec3 L, float rough, float F0) {
  vec3 H = normalize(V + L);
  float NdotV = max(dot(N, V), 0.0);
  float NdotL = max(dot(N, L), 0.0);
  float NdotH = max(dot(N, H), 0.0);
  float VdotH = max(dot(V, H), 0.0);

  float a = max(rough * rough, 0.001);
  float a2 = a * a;
  float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
  float D = a2 / max(PI * denom * denom, 1e-4);

  float k = (a + 1.0);
  k = (k * k) * 0.125;
  float Gv = NdotV / (NdotV * (1.0 - k) + k);
  float Gl = NdotL / (NdotL * (1.0 - k) + k);
  float G = Gv * Gl;

  float F = fresnel_schlick(VdotH, F0);
  return (D * G * F) / max(4.0 * NdotV * NdotL, 1e-4);
}

void main() {
  vec3 Ng = normalize(v_normal);
  float upward = smoothstep(0.55, 0.92, Ng.y);
  float dressed = float(v_dressed_stone);
  float paving = upward * (1.0 - dressed);

  vec2 block_size = mix(vec2(0.92, 0.38), vec2(0.43, 0.31), paving);
  vec2 uv = v_tex_coord / block_size;
  float row = floor(uv.y);
  uv.x += mod(row, 2.0) * 0.5;
  vec2 cell = floor(uv);
  float cell_rnd = soi_hash_82bbee(cell + vec2(13.0, 41.0));
  vec2 local = fract(uv);
  vec2 edge_distance = min(local, 1.0 - local) * block_size;

  vec2 mineral_pos = v_world_pos.xz + v_world_pos.y * vec2(0.73, -0.41);
  float mineral = fbm(mineral_pos * 1.7);
  float weather = fbm(mineral_pos * 0.23 + vec2(19.0, -7.0));
  float chips = soi_noise_3d41e6(v_tex_coord * 24.0 + cell_rnd * 7.0);
  float edge = min(edge_distance.x, edge_distance.y) - (chips - 0.5) * 0.009;
  float aa = max(fwidth(edge), 0.001);
  float stone_mask = smoothstep(0.009 - aa, 0.020 + aa, edge);
  float bevel = smoothstep(0.013, 0.043, edge);

  float distance_fade = smoothstep(0.12, 0.65, max(fwidth(uv.x), fwidth(uv.y)));
  stone_mask = mix(stone_mask, 0.88, distance_fade);
  stone_mask = mix(stone_mask, 1.0, dressed);
  bevel = mix(bevel, 1.0, dressed);
  float mortar_mask = 1.0 - stone_mask;

  float hue = soi_hash_82bbee(cell + vec2(71.0, 9.0));
  vec3 warm_stone = vec3(1.08, 1.025, 0.92);
  vec3 cool_stone = vec3(0.88, 0.94, 1.015);
  vec3 tint = mix(warm_stone, cool_stone, hue * 0.58);
  tint = mix(tint, vec3(1.055, 1.035, 0.99), dressed);
  float block_variation = mix((cell_rnd - 0.5) * 0.24, (weather - 0.5) * 0.15, dressed);
  float grain_fade = 1.0 - smoothstep(0.035, 0.18, length(fwidth(mineral_pos)));
  float grain = (soi_noise_3d41e6(mineral_pos * 48.0) - 0.5) * grain_fade;
  vec3 stone_color = u_color * tint *
                     (1.0 + block_variation + (mineral - 0.45) * 0.20 + grain * 0.055);
  stone_color *= 1.0 + dressed * 0.095;
  vec3 mortar_color = u_color * vec3(0.65, 0.65, 0.62);
  vec3 base_color = mix(mortar_color, stone_color, stone_mask);

  float arris = (1.0 - bevel) * stone_mask * (1.0 - distance_fade);
  base_color += u_color * arris * 0.11;
  float vein =
      1.0 - smoothstep(0.018, 0.065, abs(mineral - 0.49 + (weather - 0.5) * 0.16));
  base_color *= 1.0 - vein * stone_mask * 0.055;
  float pits = smoothstep(0.72, 0.88, soi_noise_3d41e6(mineral_pos * 32.0));
  base_color *= 1.0 - pits * grain_fade * 0.12;

  float streaks = fbm(vec2(v_tex_coord.x * 3.2, v_tex_coord.y * 0.18));
  float sheltered = (1.0 - upward) * smoothstep(0.42, 0.72, weather);
  float runoff = sheltered * smoothstep(0.40, 0.72, streaks);
  base_color *= 1.0 - runoff * 0.19;
  float moss = smoothstep(0.52, 0.73, weather) * smoothstep(0.38, 0.65, mineral) *
               (mortar_mask * 0.65 + (1.0 - bevel) * 0.18) * (1.0 - dressed);
  base_color = mix(base_color, u_color * vec3(0.43, 0.51, 0.29), moss * 0.65);

  float height = (bevel * 0.024 + (mineral - 0.5) * 0.013 + grain * 0.003) *
                 mix(1.0 - distance_fade, 1.0, dressed);
  vec3 N = relief_normal(Ng, height);
  float ao = mix(0.72, 1.0, bevel) * (1.0 - runoff * 0.07);

  float wetness = environment_wetness();
  float rain_exposure = smoothstep(0.08, 0.82, Ng.y);
  float joint_pool =
      wetness * rain_exposure * mortar_mask * smoothstep(0.36, 0.66, weather);
  float damp = wetness * (0.24 + rain_exposure * 0.56 + runoff * 0.20);
  base_color *= 1.0 - damp * 0.25 - joint_pool * 0.12;

  vec3 wet_normal = normalize(mix(N, Ng, damp * 0.18 + joint_pool * 0.60));
  float roughness = clamp(0.84 - paving * 0.09 - dressed * 0.06 + pits * 0.08 -
                              damp * 0.27 - joint_pool * 0.27,
                          0.18,
                          0.96);
  float F0 = mix(0.035, 0.05, damp);
  vec3 L = environment_primary_direction();
  vec3 V = normalize(u_camera_pos - v_world_pos);
  float spec = ggx_specular(wet_normal, V, L, roughness, F0);
  float sky_fresnel = fresnel_schlick(max(dot(wet_normal, V), 0.0), F0);

  vec3 lit_color = base_color * soi_surface_lighting_scaled(wet_normal, 0.76) * ao;
  lit_color += environment_primary_color() * environment_primary_intensity() * spec *
               max(dot(wet_normal, L), 0.0) * (0.24 + damp * 0.50 + joint_pool * 0.42);
  lit_color +=
      environment_sky_color() * sky_fresnel * (damp * 0.065 + joint_pool * 0.10);
  lit_color += soi_rim_light(wet_normal, V) * 0.65;
  lit_color = apply_directional_shadow(lit_color, v_world_pos, v_normal);
  lit_color += base_color * ao * local_lighting(v_world_pos, wet_normal);

  vec2 reveal_sample = fog_reveal_sample(v_world_pos.xz);
  float reveal_alpha = fog_reveal_alpha(reveal_sample.x);
  VisibilityMask vis;
  vis.seen_now = reveal_sample.y;
  vis.known = reveal_sample.x;
  if (fog_reveal_active()) {
    vec3 memory = remembered_surface_color(lit_color, u_explored_alpha) *
                  visibility_memory_falloff(vis);
    lit_color = mix(memory, lit_color, visibility_live_weight(vis));
  }
  lit_color = fog_reveal_haze(lit_color, reveal_alpha);
  frag_color = vec4(lit_color, 1.0);
}
