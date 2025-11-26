#version 330 core

in vec3 v_normal;
in vec2 v_tex_coord;
in vec3 v_world_pos;

uniform vec3 u_color;           // base road color
uniform vec3 u_light_direction; // world-space light direction
uniform float u_alpha;          // transparency (for fog-of-war)

out vec4 frag_color;

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
const float PI = 3.14159265359;

float saturate_val(float x) { return clamp(x, 0.0, 1.0); }

mat2 rotate_2d(float a) {
  float c = cos(a), s = sin(a);
  return mat2(c, -s, s, c);
}

// Hash / Noise functions
float hash_2d(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

float noise_2d(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float a = hash_2d(i);
  float b = hash_2d(i + vec2(1.0, 0.0));
  float c = hash_2d(i + vec2(0.0, 1.0));
  float d = hash_2d(i + vec2(1.0, 1.0));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm_2d(vec2 p) {
  float v = 0.0, a = 0.5;
  for (int i = 0; i < 5; ++i) {
    v += a * noise_2d(p);
    p *= 2.0;
    a *= 0.5;
  }
  return v;
}

// 2D random vector for Worley
vec2 hash_2d_vec(vec2 p) {
  float n = sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123;
  return fract(vec2(n, n * 1.2154));
}

// Worley (Voronoi) for stone pattern: returns F1 and F2
vec2 worley_f(vec2 p) {
  vec2 n = floor(p);
  vec2 f = fract(p);
  float f1 = 1e9;
  float f2 = 1e9;
  for (int j = -1; j <= 1; ++j) {
    for (int i = -1; i <= 1; ++i) {
      vec2 g = vec2(float(i), float(j));
      vec2 o = hash_2d_vec(n + g);
      vec2 r = (g + o) - f;
      float d = dot(r, r);
      if (d < f1) {
        f2 = f1;
        f1 = d;
      } else if (d < f2) {
        f2 = d;
      }
    }
  }
  return vec2(sqrt(f1), sqrt(f2));
}

// Fresnel (Schlick)
float fresnel_schlick(float cos_theta, float f0) {
  return f0 + (1.0 - f0) * pow(1.0 - cos_theta, 5.0);
}

// GGX specular
float ggx_specular(vec3 n_vec, vec3 v_vec, vec3 l_vec, float rough, float f0) {
  vec3 h_vec = normalize(v_vec + l_vec);
  float n_dot_v = max(dot(n_vec, v_vec), 0.0);
  float n_dot_l = max(dot(n_vec, l_vec), 0.0);
  float n_dot_h = max(dot(n_vec, h_vec), 0.0);
  float v_dot_h = max(dot(v_vec, h_vec), 0.0);

  float a = max(rough * rough, 0.001);
  float a2 = a * a;
  float denom = (n_dot_h * n_dot_h * (a2 - 1.0) + 1.0);
  float d_val = a2 / max(PI * denom * denom, 1e-4);

  float k = (a + 1.0);
  k = (k * k) * 0.125;
  float g_v = n_dot_v / (n_dot_v * (1.0 - k) + k);
  float g_l = n_dot_l / (n_dot_l * (1.0 - k) + k);
  float g_val = g_v * g_l;

  float f_val = fresnel_schlick(v_dot_h, f0);
  return (d_val * g_val * f_val) / max(4.0 * n_dot_v * n_dot_l, 1e-4);
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------
void main() {
  // World-anchored UV mapping for consistent stone pattern
  vec2 uv = v_world_pos.xz * 0.8;

  // Irregular cobblestone layout via Worley noise
  vec2 worley_result = worley_f(uv * 1.5);
  float edge_metric = worley_result.y - worley_result.x;
  float stone_mask = smoothstep(0.04, 0.25, edge_metric);
  float mortar_mask = 1.0 - stone_mask;

  // Per-cell variation for stone shapes
  vec2 cell = floor(uv * 1.5);
  float cell_rnd = hash_2d(cell);
  vec2 local = fract(uv);
  vec2 uv_var =
      (rotate_2d(cell_rnd * 6.2831853) * (local - 0.5) + 0.5) + floor(uv);

  // Albedo variation - Roman roads have weathered stone look
  float var_low = (fbm_2d(uv * 0.4) - 0.5) * 0.18;
  float var_mid = (fbm_2d(uv_var * 2.5) - 0.5) * 0.12;
  float grain = (noise_2d(uv_var * 15.0) - 0.5) * 0.06;

  // Base stone color with Roman road characteristics (brownish-gray)
  vec3 stone_base = u_color;
  vec3 stone_color = stone_base * (1.0 + var_low + var_mid + grain);

  // Mortar/gravel between stones - darker and rougher
  vec3 mortar_color = stone_base * 0.60;

  // Wear patterns - roads show wear in the center from traffic
  float center_wear = 1.0 - abs(v_tex_coord.x - 0.5) * 1.8;
  center_wear = smoothstep(0.0, 0.5, center_wear) * 0.08;
  stone_color *= (1.0 + center_wear);

  // Micro cracks in stones
  float crack = smoothstep(0.02, 0.0, abs(noise_2d(uv * 12.0) - 0.5)) * 0.20;
  stone_color *= (1.0 - crack * stone_mask);

  // Ambient occlusion in mortar grooves
  float cavity = smoothstep(0.0, 0.20, edge_metric);
  float ao = mix(0.50, 1.0, cavity) * (0.90 + 0.10 * fbm_2d(uv * 2.0));

  // Height variation for normal perturbation
  float micro_bump = (fbm_2d(uv_var * 3.5) - 0.5) * 0.05 * stone_mask;
  float macro_warp = (fbm_2d(uv * 1.0) - 0.5) * 0.025 * stone_mask;
  float mortar_dip = -0.05 * mortar_mask;
  float h = micro_bump + macro_warp + mortar_dip;

  // Screen-space normal perturbation
  float sx = dFdx(h);
  float sy = dFdy(h);
  float bump_strength = 12.0;
  vec3 n_bump = normalize(vec3(-sx * bump_strength, 1.0, -sy * bump_strength));

  // Blend with geometric normal
  vec3 n_geom = normalize(v_normal);
  vec3 n_final = normalize(mix(n_geom, n_bump, 0.60));

  // Lighting
  vec3 light_dir = normalize(u_light_direction);
  vec3 view_dir = normalize(vec3(0.0, 0.9, 0.4)); // Fixed view direction

  // Diffuse (Lambert) with AO
  float n_dot_l = max(dot(n_final, light_dir), 0.0);
  float diffuse = n_dot_l;

  // Roughness - worn stones are smoother, edges rougher
  float steep = saturate_val(length(vec2(sx, sy)) * bump_strength);
  float roughness = clamp(mix(0.70, 0.95, steep), 0.02, 1.0);
  float f0 = 0.03; // Dielectric stone

  float spec = ggx_specular(n_final, view_dir, light_dir, roughness, f0);

  // Final base color: blend stone and mortar
  vec3 base_color = mix(mortar_color, stone_color, stone_mask);

  // Hemispheric ambient lighting
  vec3 hemi_sky = vec3(0.20, 0.25, 0.30);
  vec3 hemi_ground = vec3(0.10, 0.09, 0.08);
  float hemi = n_final.y * 0.5 + 0.5;

  vec3 lit_color = base_color * (0.40 + 0.65 * diffuse) * ao;
  lit_color += mix(hemi_ground, hemi_sky, hemi) * 0.12;
  lit_color += vec3(1.0) * spec * 0.20;

  // Dirt accumulation in grooves
  float grime = (1.0 - cavity) * 0.20 * (0.8 + 0.2 * noise_2d(uv * 6.0));
  float gray = dot(lit_color, vec3(0.299, 0.587, 0.114));
  lit_color = mix(lit_color, vec3(gray * 0.85), grime);

  // Edge darkening for road borders
  float edge_fade = smoothstep(0.0, 0.08, v_tex_coord.x) *
                    smoothstep(0.0, 0.08, 1.0 - v_tex_coord.x);
  lit_color *= mix(0.75, 1.0, edge_fade);

  frag_color = vec4(clamp(lit_color, 0.0, 1.0), u_alpha);
}
