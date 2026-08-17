#version 330 core
#include "directional_shadows.glsl"
#include "environment_lighting.glsl"
#include "local_lighting.glsl"
#include "noise.glsl"

in vec3 v_normal;
in vec2 v_tex_coord;
in vec3 v_world_pos;

uniform vec3 u_color;

out vec4 frag_color;

const float PI = 3.14159265359;

float saturate(float x) {
  return clamp(x, 0.0, 1.0);
}

mat2 rot(float a) {
  float c = cos(a), s = sin(a);
  return mat2(c, -s, s, c);
}

float fbm(vec2 p) {
  float v = 0.0, a = 0.5;
  for (int i = 0; i < 5; ++i) {
    v += a * soi_noise_3d41e6(p);
    p *= 2.0;
    a *= 0.5;
  }
  return v;
}

vec2 hash2(vec2 p) {
  float n = sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123;
  return fract(vec2(n, n * 1.2154));
}

vec2 worley_f(vec2 p) {
  vec2 n = floor(p);
  vec2 f = fract(p);
  float F1 = 1e9;
  float F2 = 1e9;
  for (int j = -1; j <= 1; ++j) {
    for (int i = -1; i <= 1; ++i) {
      vec2 g = vec2(float(i), float(j));
      vec2 o = hash2(n + g);
      vec2 r = (g + o) - f;
      float d = dot(r, r);
      if (d < F1) {
        F2 = F1;
        F1 = d;
      } else if (d < F2) {
        F2 = d;
      }
    }
  }
  return vec2(sqrt(F1), sqrt(F2));
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

  const float k_setts_per_metre = 2.7;

  vec3 Ng = normalize(v_normal);
  vec3 axial = abs(Ng);
  vec2 surface_pos;
  vec3 face_normal;
  vec3 axis_u;
  vec3 axis_v;
  if (axial.y >= max(axial.x, axial.z)) {
    surface_pos = v_world_pos.xz;
    face_normal = vec3(0.0, Ng.y >= 0.0 ? 1.0 : -1.0, 0.0);
    axis_u = vec3(1.0, 0.0, 0.0);
    axis_v = vec3(0.0, 0.0, 1.0);
  } else if (axial.x >= axial.z) {
    surface_pos = v_world_pos.zy;
    face_normal = vec3(Ng.x >= 0.0 ? 1.0 : -1.0, 0.0, 0.0);
    axis_u = vec3(0.0, 0.0, 1.0);
    axis_v = vec3(0.0, 1.0, 0.0);
  } else {
    surface_pos = v_world_pos.xy;
    face_normal = vec3(0.0, 0.0, Ng.z >= 0.0 ? 1.0 : -1.0);
    axis_u = vec3(1.0, 0.0, 0.0);
    axis_v = vec3(0.0, 1.0, 0.0);
  }

  vec2 uv = surface_pos * k_setts_per_metre;
  vec2 macro_uv = surface_pos * 0.45;

  vec2 F = worley_f(uv);
  float edge_metric = F.y - F.x;
  float stone_mask = smoothstep(0.030, 0.165, edge_metric);
  float mortar_mask = 1.0 - stone_mask;

  vec2 cell = floor(uv);
  float cell_rnd = soi_hash_82bbee(cell);
  vec2 local = fract(uv);
  vec2 uv_var = (rot(cell_rnd * 6.2831853) * (local - 0.5) + 0.5) + cell;

  float var_low = (fbm(macro_uv) - 0.5) * 0.20;
  float var_mid = (cell_rnd - 0.5) * 0.30;
  float grain = (soi_noise_3d41e6(uv_var * 9.0) - 0.5) * 0.08;

  float cell_hue = soi_hash_82bbee(cell + vec2(37.0, -19.0)) - 0.5;
  vec3 stone_tint = vec3(1.0 + cell_hue * 0.10, 1.0, 1.0 - cell_hue * 0.09);

  vec3 stone_color = u_color * stone_tint * (1.0 + var_low + var_mid + grain);
  vec3 mortar_color = u_color * 0.80;

  float crack = smoothstep(0.02, 0.0, abs(soi_noise_3d41e6(uv * 2.4) - 0.5)) * 0.22;
  stone_color *= (1.0 - crack * stone_mask);

  float cavity = smoothstep(0.0, 0.10, edge_metric);
  float ao = mix(0.70, 1.0, cavity) * (0.93 + 0.07 * fbm(macro_uv * 3.0));

  float micro_bump = (fbm(uv_var * 2.0) - 0.5) * 0.06 * stone_mask;
  float macro_warp = (fbm(macro_uv * 2.5) - 0.5) * 0.03 * stone_mask;
  float mortar_dip = -0.06 * mortar_mask;
  float h = micro_bump + macro_warp + mortar_dip;

  float sx = dFdx(h);
  float sy = dFdy(h);
  float bump_strength = 14.0;

  vec2 slope = vec2(sx, sy) * bump_strength;
  slope /= 1.0 + length(slope);
  vec3 n_bump = normalize(face_normal - (axis_u * slope.x + axis_v * slope.y));

  vec3 N = normalize(mix(Ng, n_bump, 0.65));

  vec3 L = environment_primary_direction();
  vec3 V = normalize(vec3(0.0, 0.9, 0.4));

  float steep = saturate(length(slope));
  float roughness = clamp(mix(0.65, 0.95, steep), 0.02, 1.0);
  float F0 = 0.035;

  float spec = ggx_specular(N, V, L, roughness, F0);

  vec3 base_color = mix(mortar_color, stone_color, stone_mask);

  vec3 lit_color = base_color * soi_surface_lighting_scaled(N, 0.76) * ao;
  lit_color += environment_primary_color() * spec * 0.14;
  lit_color += soi_rim_light(N, V);

  float grime = (1.0 - cavity) * 0.25 * (0.8 + 0.2 * soi_noise_3d41e6(macro_uv * 5.0));
  float gray = dot(lit_color, vec3(0.299, 0.587, 0.114));
  lit_color = mix(lit_color, vec3(gray * 0.9), grime);

  lit_color += base_color * ao * local_lighting(v_world_pos, Ng);
  lit_color = apply_directional_shadow(lit_color, v_world_pos, v_normal);
  frag_color = vec4(lit_color, 1.0);
}
