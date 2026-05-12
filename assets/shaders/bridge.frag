#version 330 core

in vec3 v_normal;
in vec2 v_tex_coord;
in vec3 v_world_pos;

uniform vec3 u_color;
uniform vec3 u_light_direction;
uniform sampler2_d u_fog_texture;

out vec4 frag_color;

const float PI = 3.14159265359;

float saturate(float x) { return clamp(x, 0.0, 1.0); }

mat2 rot(float a) {
  float c = cos(a), s = sin(a);
  return mat2(c, -s, s, c);
}

float hash(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}
float noise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float a = hash(i);
  float b = hash(i + vec2(1.0, 0.0));
  float c = hash(i + vec2(0.0, 1.0));
  float d = hash(i + vec2(1.0, 1.0));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
float fbm(vec2 p) {
  float v = 0.0, a = 0.5;
  for (int i = 0; i < 5; ++i) {
    v += a * noise(p);
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

  vec2 uv = v_world_pos.xz * 0.6;

  vec2 F = worley_f(uv * 1.2);
  float edge_metric = F.y - F.x;
  float stone_mask = smoothstep(0.05, 0.30, edge_metric);
  float mortar_mask = 1.0 - stone_mask;

  vec2 cell = floor(uv * 1.2);
  float cell_rnd = hash(cell);
  vec2 local = fract(uv);
  vec2 uv_var = (rot(cell_rnd * 6.2831853) * (local - 0.5) + 0.5) + floor(uv);

  float var_low = (fbm(uv * 0.5) - 0.5) * 0.20;
  float var_mid = (fbm(uv_var * 3.0) - 0.5) * 0.15;
  float grain = (noise(uv_var * 18.0) - 0.5) * 0.08;

  vec3 stone_color = u_color * (1.0 + var_low + var_mid + grain);
  vec3 mortar_color = u_color * 0.55;

  float crack = smoothstep(0.02, 0.0, abs(noise(uv * 10.0) - 0.5)) * 0.25;
  stone_color *= (1.0 - crack * stone_mask);

  float cavity = smoothstep(0.0, 0.18, edge_metric);
  float ao = mix(0.55, 1.0, cavity) * (0.92 + 0.08 * fbm(uv * 2.5));

  float micro_bump = (fbm(uv_var * 4.0) - 0.5) * 0.06 * stone_mask;
  float macro_warp = (fbm(uv * 1.2) - 0.5) * 0.03 * stone_mask;
  float mortar_dip = -0.06 * mortar_mask;
  float h = micro_bump + macro_warp + mortar_dip;

  float sx = dFdx(h);
  float sy = dFdy(h);
  float bump_strength = 14.0;
  vec3 n_bump = normalize(vec3(-sx * bump_strength, 1.0, -sy * bump_strength));

  vec3 Ng = normalize(v_normal);
  vec3 N = normalize(mix(Ng, n_bump, 0.65));

  vec3 L = normalize(u_light_direction);
  vec3 V = normalize(vec3(0.0, 0.9, 0.4));

  float NdotL = max(dot(N, L), 0.0);
  float diffuse = NdotL;

  float steep = saturate(length(vec2(sx, sy)) * bump_strength);
  float roughness = clamp(mix(0.65, 0.95, steep), 0.02, 1.0);
  float F0 = 0.035;

  float spec = ggx_specular(N, V, L, roughness, F0);

  vec3 base_color = mix(mortar_color, stone_color, stone_mask);

  vec3 hemi_sky = vec3(0.18, 0.24, 0.30);
  vec3 hemi_ground = vec3(0.12, 0.10, 0.09);
  float hemi = N.y * 0.5 + 0.5;

  vec3 lit_color = base_color * (0.35 + 0.70 * diffuse) * ao;
  lit_color += mix(hemi_ground, hemi_sky, hemi) * 0.15;
  lit_color += vec3(1.0) * spec * 0.25;

  float grime = (1.0 - cavity) * 0.25 * (0.8 + 0.2 * noise(uv * 7.0));
  float gray = dot(lit_color, vec3(0.299, 0.587, 0.114));
  lit_color = mix(lit_color, vec3(gray * 0.9), grime);

  float fog_mask = texture(u_fog_texture, v_tex_coord).r;
  float fog_amt = 1.0 - fog_mask;
  lit_color *= mix(1.0, 0.85, fog_amt * 0.5);

  frag_color = vec4(lit_color, 1.0);
}
