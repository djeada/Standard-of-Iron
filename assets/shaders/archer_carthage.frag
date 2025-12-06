#version 330 core

in vec3 v_normal;
in vec3 v_worldNormal;
in vec3 v_tangent;
in vec3 v_bitangent;
in vec2 v_texCoord;
in vec3 v_worldPos;
in float v_armorLayer;
in float v_leatherTension;
in float v_bodyHeight;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_useTexture;
uniform float u_alpha;
uniform float u_time;
uniform float u_rainIntensity;
uniform int u_materialId;

out vec4 FragColor;

// ----------------------------- Utils & Noise -----------------------------
const float PI = 3.14159265359;

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

float triplanar_noise(vec3 pos, vec3 normal, float scale) {
  vec3 b = abs(normal);
  b = max(b, vec3(0.0001));
  b /= (b.x + b.y + b.z);
  float xy = noise(pos.xy * scale);
  float yz = noise(pos.yz * scale);
  float zx = noise(pos.zx * scale);
  return xy * b.z + yz * b.x + zx * b.y;
}

float fbm(vec3 pos, vec3 normal, float scale) {
  float total = 0.0;
  float amp = 0.5;
  float freq = 1.0;
  for (int i = 0; i < 5; ++i) {
    total += amp * triplanar_noise(pos * freq, normal, scale * freq);
    freq *= 2.02;
    amp *= 0.45;
  }
  return total;
}

struct MaterialSample {
  vec3 albedo;
  vec3 normal;
  float roughness;
  float ao;
  float metallic;
  vec3 F0;
};

struct Light {
  vec3 dir;
  vec3 color;
  float intensity;
};

const vec3 REF_LEATHER = vec3(0.38, 0.26, 0.14);
const vec3 REF_LEATHER_DARK = vec3(0.24, 0.16, 0.10);
const vec3 REF_WOOD = vec3(0.38, 0.28, 0.18);
const vec3 REF_CLOTH = vec3(0.14, 0.34, 0.52);
const vec3 REF_SKIN = vec3(0.93, 0.78, 0.65);
const vec3 REF_BEARD = vec3(0.28, 0.20, 0.13);
const vec3 REF_METAL = vec3(0.75, 0.75, 0.78);
const vec3 CANON_WOOD = vec3(0.24, 0.16, 0.09);
const vec3 CANON_SKIN = vec3(0.92, 0.78, 0.64);
const vec3 CANON_BEARD = vec3(0.05, 0.05, 0.05);
const vec3 CANON_HELMET = vec3(0.78, 0.80, 0.88);

// Luma / chroma helpers
float luma(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }
float sat(vec3 c) {
  float mx = max(max(c.r, c.g), c.b);
  float mn = min(min(c.r, c.g), c.b);
  return (mx - mn) / max(mx, 1e-5);
}

float color_distance(vec3 a, vec3 b) { return length(a - b); }

// ----------------------------- Geometry helpers -----------------------------
float fresnel_schlick(float cos_theta, float F0) {
  return F0 + (1.0 - F0) * pow(1.0 - cos_theta, 5.0);
}

vec3 fresnel_schlick(vec3 F0, float cos_theta) {
  return F0 + (vec3(1.0) - F0) * pow(1.0 - cos_theta, 5.0);
}

float compute_curvature(vec3 normal) {
  vec3 dx = dFdx(normal);
  vec3 dy = dFdy(normal);
  return length(dx) + length(dy);
}

// Microfacet (GGX) – isotropic (we’ll tint/spec weight per material)
float D_GGX(float NdotH, float a) {
  float a2 = a * a;
  float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
  return a2 / max(PI * d * d, 1e-6);
}

float G_Smith(float NdotV, float NdotL, float a) {
  // Schlick-GGX
  float k = (a + 1.0);
  k = (k * k) / 8.0;
  float g_v = NdotV / (NdotV * (1.0 - k) + k);
  float g_l = NdotL / (NdotL * (1.0 - k) + k);
  return g_v * g_l;
}

// Perturb normal procedurally to add micro detail per material
vec3 perturb_normal_leather(vec3 N, vec3 T, vec3 B, vec3 P) {
  float g1 = fbm(P * 6.0, N, 8.0);
  float g2 = fbm(P * 18.0 + vec3(2.1), N, 24.0);
  vec3 n_t = T * (g1 * 0.06 + g2 * 0.02);
  vec3 n_b = B * (g1 * 0.03 + g2 * 0.03);
  vec3 p_n = normalize(N + n_t + n_b);
  return p_n;
}

vec3 perturb_normal_linen(vec3 N, vec3 T, vec3 B, vec3 P) {
  // warp/weft weave
  float warp = sin(P.x * 140.0) * 0.06;
  float weft = sin(P.z * 146.0) * 0.06;
  float slub = fbm(P * 7.0, N, 10.0) * 0.04;
  vec3 p_n = normalize(N + T * (warp + slub) + B * (weft + slub * 0.5));
  return p_n;
}

vec3 perturb_normal_bronze(vec3 N, vec3 T, vec3 B, vec3 P) {
  float hammer = fbm(P * 16.0, N, 22.0) * 0.10;
  float ripple = fbm(P * 40.0 + vec3(3.7), N, 55.0) * 0.03;
  vec3 p_n = normalize(N + T * hammer + B * ripple);
  return p_n;
}

// Clearcoat (water film) lobe for rain
vec3 clearcoat_spec(vec3 N, vec3 L, vec3 V, float coat_strength,
                    float coat_rough) {
  vec3 H = normalize(L + V);
  float NdotV = max(dot(N, V), 0.0);
  float NdotL = max(dot(N, L), 0.0);
  float NdotH = max(dot(N, H), 0.0);
  float a = max(coat_rough, 0.02);
  float D = D_GGX(NdotH, a);
  float G = G_Smith(NdotV, NdotL, a);
  // IOR ~1.33 → F0 ≈ 0.02
  float F = fresnel_schlick(max(dot(H, V), 0.0), 0.02);
  float spec = (D * G * F) / max(4.0 * NdotV * NdotL + 1e-5, 1e-5);
  return vec3(spec * coat_strength);
}

mat3 make_tangent_basis(vec3 N, vec3 T, vec3 B) {
  vec3 t = normalize(T - N * dot(N, T));
  vec3 b = normalize(B - N * dot(N, B));
  if (length(t) < 1e-4)
    t = vec3(1.0, 0.0, 0.0);
  if (length(b) < 1e-4)
    b = normalize(cross(N, t));
  return mat3(t, b, N);
}

vec3 apply_micro_normal(vec3 base_n, vec3 T, vec3 B, vec3 pos,
                        float intensity) {
  mat3 basis = make_tangent_basis(base_n, T, B);
  float noise_x = fbm(pos * 18.0 + vec3(1.37, 2.07, 3.11), base_n, 24.0);
  float noise_y = fbm(pos * 20.0 + vec3(-2.21, 1.91, 0.77), base_n, 26.0);
  vec3 tangent_normal =
      normalize(vec3(noise_x * 2.0 - 1.0, noise_y * 2.0 - 1.0, 1.0));
  tangent_normal.xy *= intensity;
  return normalize(basis * tangent_normal);
}

vec3 tone_map_and_gamma(vec3 color) {
  color = color / (color + vec3(1.0));
  return pow(color, vec3(1.0 / 2.2));
}

vec3 compute_ambient(vec3 normal) {
  float up = clamp(normal.y, 0.0, 1.0);
  float down = clamp(-normal.y, 0.0, 1.0);
  vec3 sky = vec3(0.60, 0.70, 0.85);
  vec3 ground = vec3(0.40, 0.34, 0.28);
  return sky * (0.25 + 0.55 * up) + ground * (0.10 + 0.35 * down);
}

MaterialSample make_leather_sample(vec3 base_color, vec3 Nw, vec3 Tw, vec3 Bw,
                                   vec3 world_pos, float tension,
                                   float body_height, float layer,
                                   float wet_mask, float curvature) {
  MaterialSample mat;
  mat.metallic = 0.0;

  vec3 target_leather = vec3(0.42, 0.30, 0.20);
  vec3 color = mix(base_color, target_leather, 0.12);

  float torso_bleach = mix(0.92, 1.08, clamp(body_height, 0.0, 1.0));
  color *= vec3(torso_bleach, mix(0.90, 0.98, body_height),
                mix(0.87, 0.95, body_height));

  float macro = fbm(world_pos * 3.2, Nw, 4.2);
  float medium = fbm(world_pos * 7.6, Nw, 8.5);
  float fine = fbm(world_pos * 16.0, Nw, 18.0);
  float pores = fbm(world_pos * 32.0 + vec3(3.7), Nw, 38.0);
  float albedo_noise =
      macro * 0.35 + medium * 0.30 + fine * 0.22 + pores * 0.13;
  color *= mix(0.88, 1.12, albedo_noise);

  float dirt = fbm(world_pos * vec3(2.5, 1.1, 2.5), Nw, 3.5) *
               (1.0 - clamp(body_height, 0.0, 1.0));
  color = mix(color, color * vec3(0.70, 0.58, 0.42),
              smoothstep(0.45, 0.75, dirt) * 0.25);

  float salt =
      smoothstep(0.62, 0.95, fbm(world_pos * vec3(12.0, 6.0, 12.0), Nw, 14.0));
  color = mix(color, color * vec3(0.82, 0.80, 0.76), salt * 0.18);

  float strap_band = smoothstep(0.1, 0.9, sin(world_pos.y * 5.2 + layer * 1.7));
  float seam = sin(world_pos.x * 3.7 + world_pos.z * 2.9);
  color = mix(color, color * vec3(0.86, 0.83, 0.78), strap_band * 0.12);
  color = mix(color, base_color, smoothstep(0.2, 0.8, seam) * 0.08);

  color = mix(color, color * 0.55, wet_mask * 0.85);

  vec3 macro_normal = perturb_normal_leather(Nw, Tw, Bw, world_pos);
  vec3 normal = apply_micro_normal(macro_normal, Tw, Bw, world_pos, 0.55);

  float grain = macro * 0.40 + medium * 0.32 + fine * 0.20 + pores * 0.08;
  float rough_base = clamp(0.78 - tension * 0.18 + grain * 0.15, 0.52, 0.92);
  float wet_influence = mix(0.0, -0.28, wet_mask);
  mat.roughness = clamp(rough_base + wet_influence, 0.35, 0.95);

  float crease =
      smoothstep(0.45, 0.75, fbm(world_pos * vec3(1.4, 3.5, 1.4), Nw, 2.6));
  float layer_ao = mix(0.85, 0.65, clamp(layer / 2.0, 0.0, 1.0));
  float curvature_ao = mix(0.75, 1.0, clamp(1.0 - curvature * 1.2, 0.0, 1.0));
  mat.ao = clamp((1.0 - crease * 0.35) * layer_ao * curvature_ao *
                     (0.9 - wet_mask * 0.15),
                 0.35, 1.0);

  mat.albedo = color;
  mat.normal = normal;
  vec3 tint_spec = mix(vec3(0.035), color, 0.16);
  mat.F0 = mix(tint_spec, vec3(0.08), wet_mask * 0.45);
  return mat;
}

MaterialSample make_linen_sample(vec3 base_color, vec3 Nw, vec3 Tw, vec3 Bw,
                                 vec3 world_pos, float body_height,
                                 float wet_mask, float curvature) {
  MaterialSample mat;
  mat.metallic = 0.0;

  vec3 target = vec3(0.88, 0.85, 0.78);
  vec3 color = mix(target, base_color, 0.45);
  float weave = sin(world_pos.x * 62.0) * sin(world_pos.z * 66.0) * 0.08;
  float sizing = fbm(world_pos * 3.0, Nw, 4.5) * 0.10;
  float fray = fbm(world_pos * 9.0, Nw, 10.0) *
               clamp(1.4 - clamp(Nw.y, 0.0, 1.0), 0.0, 1.0) * 0.12;
  color += vec3(weave * 0.35);
  color -= vec3(sizing * 0.5);
  color -= vec3(fray * 0.12);

  float dust = clamp(1.0 - Nw.y, 0.0, 1.0) * fbm(world_pos * 1.1, Nw, 2.0);
  float sweat =
      smoothstep(0.6, 1.0, body_height) * fbm(world_pos * 2.4, Nw, 3.1);
  color = mix(color, color * (1.0 - dust * 0.35), 0.7);
  color = mix(color, color * vec3(0.96, 0.93, 0.88),
              1.0 - clamp(sweat * 0.5, 0.0, 1.0));

  color *= (1.0 - wet_mask * 0.35);

  mat.albedo = color;
  mat.normal = perturb_normal_linen(Nw, Tw, Bw, world_pos);
  float rough_noise = fbm(world_pos * 5.0, Nw, 7.5);
  mat.roughness =
      clamp(0.82 + rough_noise * 0.12 - wet_mask * 0.22, 0.55, 0.96);
  mat.ao =
      clamp(0.85 - dust * 0.20 - sweat * 0.15 + curvature * 0.05, 0.4, 1.0);
  mat.F0 = vec3(0.028);
  return mat;
}

MaterialSample make_bronze_sample(vec3 base_color, vec3 Nw, vec3 Tw, vec3 Bw,
                                  vec3 world_pos, float wet_mask,
                                  float curvature) {
  MaterialSample mat;
  mat.metallic = 0.9;
  vec3 bronze_warm = vec3(0.58, 0.44, 0.20);
  vec3 cuprite = vec3(0.36, 0.18, 0.10);
  vec3 malachite = vec3(0.18, 0.45, 0.36);

  vec3 macro_normal = perturb_normal_bronze(Nw, Tw, Bw, world_pos);
  float hammer = fbm(world_pos * 14.0, Nw, 20.0) * 0.18;
  float patina = fbm(world_pos * 6.0 + vec3(5.0), Nw, 8.0) * 0.14;
  float run_off = fbm(world_pos * vec3(1.2, 3.4, 1.2), Nw, 2.2) *
                  (1.0 - clamp(Nw.y, 0.0, 1.0));

  vec3 bronze_base = mix(bronze_warm, base_color, 0.35) + vec3(hammer);
  vec3 with_cuprite =
      mix(bronze_base, cuprite,
          smoothstep(0.70, 0.95, fbm(world_pos * 9.0, Nw, 12.0)));
  vec3 color = mix(with_cuprite, malachite,
                   clamp(patina * 0.5 + run_off * 0.6, 0.0, 1.0));

  color = mix(color, color * 0.65, wet_mask * 0.6);

  mat.albedo = color;
  mat.normal = apply_micro_normal(macro_normal, Tw, Bw, world_pos, 0.35);
  mat.roughness = clamp(0.32 + hammer * 0.25 + patina * 0.15 + wet_mask * -0.18,
                        0.18, 0.75);
  mat.ao = clamp(0.85 - patina * 0.3 + curvature * 0.1, 0.45, 1.0);
  vec3 F0 = mix(vec3(0.06), clamp(bronze_warm, 0.0, 1.0), mat.metallic);
  mat.F0 = mix(F0, vec3(0.08), wet_mask * 0.3);
  return mat;
}

MaterialSample make_fallback_sample(vec3 base_color, vec3 Nw, vec3 Tw, vec3 Bw,
                                    vec3 world_pos, float wet_mask,
                                    float curvature) {
  MaterialSample mat;
  mat.albedo = base_color * (0.9 + 0.2 * fbm(world_pos * 4.0, Nw, 5.5));
  vec3 macro_normal = perturb_normal_leather(Nw, Tw, Bw, world_pos);
  mat.normal = apply_micro_normal(macro_normal, Tw, Bw, world_pos, 0.25);
  mat.roughness = clamp(0.60 - wet_mask * 0.20, 0.35, 0.85);
  mat.ao = clamp(0.8 + curvature * 0.1, 0.4, 1.0);
  mat.metallic = 0.0;
  mat.F0 = vec3(0.04);
  mat.albedo = mix(mat.albedo, mat.albedo * 0.7, wet_mask * 0.5);
  return mat;
}

MaterialSample make_wood_sample(vec3 base_color, vec3 Nw, vec3 Tw, vec3 Bw,
                                vec3 world_pos, float wet_mask,
                                float curvature) {
  MaterialSample mat;
  vec3 color = base_color;
  float grain = sin(world_pos.y * 18.0 + fbm(world_pos * 2.0, Nw, 2.5) * 3.5);
  float rings = sin(world_pos.x * 6.5 + grain * 2.0);
  float burn = fbm(world_pos * vec3(1.2, 0.6, 1.2), Nw, 1.6);
  color *= 1.0 + grain * 0.05;
  color -= burn * 0.08;
  color = mix(color, color * 0.6, wet_mask * 0.4);
  mat.albedo = color;
  vec3 macro_normal = apply_micro_normal(Nw, Tw, Bw, world_pos, 0.18);
  mat.normal = normalize(macro_normal + Tw * (grain * 0.05));
  mat.roughness =
      clamp(0.62 + fbm(world_pos * 6.0, Nw, 6.0) * 0.15 - wet_mask * 0.18, 0.35,
            0.92);
  mat.ao = clamp(0.9 - burn * 0.15 + curvature * 0.08, 0.4, 1.0);
  mat.metallic = 0.0;
  mat.F0 = vec3(0.035);
  return mat;
}

MaterialSample make_skin_sample(vec3 base_color, vec3 Nw, vec3 Tw, vec3 Bw,
                                vec3 world_pos, float wet_mask,
                                float curvature) {
  MaterialSample mat;
  vec3 color = base_color;
  float freckle = fbm(world_pos * vec3(9.0, 4.0, 9.0), Nw, 12.0);
  float blush = smoothstep(0.2, 0.9, Nw.y) * 0.08;
  color += freckle * 0.03;
  color += blush;
  color = mix(color, color * 0.9, wet_mask * 0.15);
  mat.albedo = color;
  mat.normal = apply_micro_normal(Nw, Tw, Bw, world_pos, 0.12);
  mat.roughness = clamp(0.58 + freckle * 0.1 - wet_mask * 0.15, 0.38, 0.85);
  mat.ao = clamp(0.92 - curvature * 0.15, 0.5, 1.0);
  mat.metallic = 0.0;
  mat.F0 = vec3(0.028);
  return mat;
}

MaterialSample make_hair_sample(vec3 base_color, vec3 Nw, vec3 Tw, vec3 Bw,
                                vec3 world_pos, float wet_mask) {
  MaterialSample mat;
  vec3 color = base_color * (0.9 + fbm(world_pos * 5.0, Nw, 7.0) * 0.12);
  float strand = sin(world_pos.x * 64.0) * 0.08;
  color += strand * 0.04;
  color = mix(color, color * 0.7, wet_mask * 0.35);
  mat.albedo = color;
  mat.normal = apply_micro_normal(Nw, Tw, Bw, world_pos, 0.45);
  mat.roughness = clamp(0.42 + strand * 0.05 - wet_mask * 0.18, 0.2, 0.7);
  mat.ao = 0.8;
  mat.metallic = 0.0;
  mat.F0 = vec3(0.06);
  return mat;
}

MaterialSample make_cloth_sample(vec3 base_color, vec3 Nw, vec3 Tw, vec3 Bw,
                                 vec3 world_pos, float wet_mask,
                                 float curvature) {
  MaterialSample mat;
  vec3 color = base_color;
  float weave = sin(world_pos.x * 52.0) * sin(world_pos.z * 55.0) * 0.08;
  float fold = fbm(world_pos * 3.4, Nw, 4.0) * 0.15;
  float stripe = sin(world_pos.y * 8.0 + world_pos.x * 2.0);
  float team_accent = smoothstep(0.2, 0.9, stripe);
  color += weave * 0.3;
  color += fold * 0.08;
  color = mix(color, color * 1.15, team_accent * 0.08);
  color = mix(color, color * 0.7, wet_mask * 0.3);
  mat.albedo = color;
  mat.normal = apply_micro_normal(Nw, Tw, Bw, world_pos, 0.12);
  mat.roughness =
      clamp(0.78 + weave * 0.1 + fold * 0.05 - wet_mask * 0.2, 0.45, 0.95);
  mat.ao = clamp(0.9 - fold * 0.2 + curvature * 0.05, 0.4, 1.0);
  mat.metallic = 0.0;
  mat.F0 = vec3(0.03);
  return mat;
}

MaterialSample make_cloak_sample(vec3 base_color, vec3 Nw, vec3 Tw, vec3 Bw,
                                 vec3 world_pos, float wet_mask,
                                 float curvature) {
  MaterialSample mat;
  vec3 color = base_color;

  // Fine fabric detail (high frequency weave)
  float weave = sin(world_pos.x * 200.0) * sin(world_pos.y * 200.0) * 0.03;
  float silk_sheen = fbm(world_pos * 4.0, Nw, 5.0) * 0.15;

  color += weave * 0.1;
  color += silk_sheen * 0.05;

  // Wetness darkening
  color = mix(color, color * 0.6, wet_mask * 0.5);

  mat.albedo = color;

  // Normal perturbation for fabric
  vec3 macro_normal = apply_micro_normal(Nw, Tw, Bw, world_pos, 0.15);
  mat.normal = macro_normal;

  // Roughness - silk/fine cloth is smoother than wool
  // Add some anisotropic feel via sheen noise
  mat.roughness = clamp(0.55 - silk_sheen * 0.2 - wet_mask * 0.3, 0.25, 0.9);

  mat.ao = clamp(0.9 - curvature * 0.1, 0.5, 1.0);
  mat.metallic = 0.0;
  mat.F0 = vec3(0.05); // Slightly higher F0 for silk/satin

  return mat;
}

MaterialSample make_metal_sample(vec3 base_color, vec3 Nw, vec3 Tw, vec3 Bw,
                                 vec3 world_pos, float wet_mask,
                                 float curvature) {
  MaterialSample mat;
  mat.metallic = 1.0;
  vec3 silver = vec3(0.78, 0.80, 0.88);
  vec3 color = mix(silver, base_color, 0.25);
  float hammer = fbm(world_pos * 22.0, Nw, 28.0) * 0.08;
  float scrape = fbm(world_pos * 8.0, Nw, 10.0) * 0.12;
  color += hammer * 0.2;
  color -= scrape * 0.08;
  color = mix(color, color * 0.7, wet_mask * 0.4);
  mat.albedo = color;
  mat.normal = apply_micro_normal(Nw, Tw, Bw, world_pos, 0.25);
  mat.roughness =
      clamp(0.22 + hammer * 0.12 + scrape * 0.08 - wet_mask * 0.15, 0.08, 0.5);
  mat.ao = clamp(0.85 - scrape * 0.25 + curvature * 0.08, 0.4, 1.0);
  mat.F0 = mix(vec3(0.08), color, 0.85);
  return mat;
}

vec3 evaluate_light(const MaterialSample mat, const Light light, vec3 V) {
  vec3 N = mat.normal;
  vec3 L = normalize(light.dir);
  float NdotL = max(dot(N, L), 0.0);
  if (NdotL <= 0.0)
    return vec3(0.0);

  vec3 H = normalize(V + L);
  float NdotV = max(dot(N, V), 0.0);
  float NdotH = max(dot(N, H), 0.0);

  float alpha = max(mat.roughness, 0.05);
  float NDF = D_GGX(NdotH, alpha);
  float G = G_Smith(NdotV, NdotL, alpha);
  vec3 F = fresnel_schlick(mat.F0, max(dot(H, V), 0.0));

  vec3 numerator = NDF * G * F;
  float denom = max(4.0 * NdotV * NdotL, 0.001);
  vec3 specular = numerator / denom;
  specular *= mix(vec3(1.0), mat.albedo, 0.18 * (1.0 - mat.metallic));

  vec3 k_s = F;
  vec3 k_d = (vec3(1.0) - k_s) * (1.0 - mat.metallic);
  vec3 diffuse = k_d * mat.albedo / PI;

  vec3 radiance = light.color * light.intensity;
  return (diffuse + specular) * radiance * NdotL;
}

// ----------------------------- Main -----------------------------
void main() {
  vec3 base_color = u_color;
  if (u_useTexture) {
    base_color *= texture(u_texture, v_texCoord).rgb;
  }

  float Y = luma(base_color);
  float S = sat(base_color);
  float blue_ratio = base_color.b / max(base_color.r, 0.001);

  bool likely_leather =
      (Y > 0.18 && Y < 0.65 && base_color.r > base_color.g * 1.03);
  bool likely_linen = (Y > 0.65 && S < 0.22);
  bool likely_bronze = (base_color.r > base_color.g * 1.03 &&
                        base_color.r > base_color.b * 1.10 && Y > 0.42);
  float leather_dist = min(color_distance(base_color, REF_LEATHER),
                           color_distance(base_color, REF_LEATHER_DARK));
  bool palette_leather = leather_dist < 0.18;
  bool looks_wood =
      (blue_ratio > 0.42 && blue_ratio < 0.8 && Y < 0.55 && S < 0.55) ||
      color_distance(base_color, REF_WOOD) < 0.12;
  bool looks_cloth = color_distance(base_color, REF_CLOTH) < 0.22 ||
                     (base_color.b > base_color.g * 1.25 &&
                      base_color.b > base_color.r * 1.35);
  bool looks_skin = color_distance(base_color, REF_SKIN) < 0.2 ||
                    (S < 0.35 && base_color.r > 0.55 && base_color.g > 0.35 &&
                     base_color.b > 0.28);
  bool looks_beard =
      (!looks_skin &&
       (color_distance(base_color, REF_BEARD) < 0.16 || (Y < 0.32 && S < 0.4)));
  bool looks_metal = color_distance(base_color, REF_METAL) < 0.18 ||
                     (S < 0.15 && Y > 0.4 && base_color.b > base_color.r * 0.9);

  bool prefer_leather = (palette_leather && blue_ratio < 0.42) ||
                        (likely_leather && !looks_wood && blue_ratio < 0.4);

  // Material ID: 0=body/skin, 1=armor, 2=helmet, 3=weapon, 4=shield, 5=cloak
  bool is_armor = (u_materialId == 1);
  bool is_helmet = (u_materialId == 2);
  bool is_weapon = (u_materialId == 3);
  bool is_shield = (u_materialId == 4);
  bool is_cloak = (u_materialId == 5);

  // Use material ID masks only (no fallback detection)
  bool is_helmet_region = is_helmet;
  bool is_face_region = (u_materialId == 0);

  vec3 Nw = normalize(v_worldNormal);
  vec3 Tw = normalize(v_tangent);
  vec3 Bw = normalize(v_bitangent);

  vec3 V = normalize(vec3(0.0, 0.0, 1.0));

  float rain = clamp(u_rainIntensity, 0.0, 1.0);
  float curvature = compute_curvature(Nw);

  float streak = smoothstep(0.65, 1.0,
                            sin(v_worldPos.y * 22.0 - u_time * 4.0 +
                                fbm(v_worldPos * 0.8, Nw, 1.2) * 6.283));
  float wet_gather = (1.0 - clamp(Nw.y, 0.0, 1.0)) *
                     (0.4 + 0.6 * fbm(v_worldPos * 2.0, Nw, 3.0));
  float wet_mask =
      clamp(rain * mix(0.5 * wet_gather, 1.0 * wet_gather, streak), 0.0, 1.0);

  MaterialSample material = make_fallback_sample(
      base_color, Nw, Tw, Bw, v_worldPos, wet_mask, curvature);
  if (is_cloak) {
    material = make_cloak_sample(base_color, Nw, Tw, Bw, v_worldPos, wet_mask,
                                 curvature);
  } else if (looks_metal && is_helmet_region) {
    material = make_metal_sample(CANON_HELMET, Nw, Tw, Bw, v_worldPos, wet_mask,
                                 curvature);
  } else if (looks_skin && is_face_region) {
    material = make_skin_sample(CANON_SKIN, Nw, Tw, Bw, v_worldPos, wet_mask,
                                curvature);
  } else if (looks_beard && is_face_region) {
    material = make_hair_sample(CANON_BEARD, Nw, Tw, Bw, v_worldPos, wet_mask);
  } else if (looks_wood) {
    vec3 wood_color = mix(base_color, CANON_WOOD, 0.35);
    material = make_wood_sample(wood_color, Nw, Tw, Bw, v_worldPos, wet_mask,
                                curvature);
  } else if (looks_cloth) {
    material = make_cloth_sample(base_color, Nw, Tw, Bw, v_worldPos, wet_mask,
                                 curvature);
  } else if (prefer_leather) {
    material = make_leather_sample(
        base_color, Nw, Tw, Bw, v_worldPos, clamp(v_leatherTension, 0.0, 1.0),
        clamp(v_bodyHeight, 0.0, 1.0), v_armorLayer, wet_mask, curvature);
  } else if (likely_linen) {
    material =
        make_linen_sample(base_color, Nw, Tw, Bw, v_worldPos,
                          clamp(v_bodyHeight, 0.0, 1.0), wet_mask, curvature);
  } else if (likely_bronze) {
    material = make_bronze_sample(base_color, Nw, Tw, Bw, v_worldPos, wet_mask,
                                  curvature);
  }

  Light lights[2];
  lights[0].dir = normalize(vec3(0.55, 1.15, 0.35));
  lights[0].color = vec3(1.15, 1.05, 0.95);
  lights[0].intensity = 1.35;
  lights[1].dir = normalize(vec3(-0.35, 0.65, -0.45));
  lights[1].color = vec3(0.35, 0.45, 0.65);
  lights[1].intensity = 0.35;

  vec3 light_accum = vec3(0.0);
  for (int i = 0; i < 2; ++i) {
    vec3 contribution = evaluate_light(material, lights[i], V);
    if (wet_mask > 0.001) {
      contribution +=
          clearcoat_spec(material.normal, lights[i].dir, V, wet_mask * 0.8,
                         mix(0.10, 0.03, wet_mask)) *
          lights[i].color * lights[i].intensity;
    }
    light_accum += contribution;
  }

  vec3 ambient =
      compute_ambient(material.normal) * material.albedo * material.ao * 0.35;
  vec3 bounce = vec3(0.45, 0.34, 0.25) *
                (0.15 + 0.45 * clamp(-material.normal.y, 0.0, 1.0));
  vec3 color =
      light_accum + ambient + bounce * (1.0 - material.metallic) * 0.25;

  color = mix(color, color * 0.85, wet_mask * 0.2);
  color = tone_map_and_gamma(max(color, vec3(0.0)));

  FragColor = vec4(clamp(color, 0.0, 1.0), u_alpha);
}
