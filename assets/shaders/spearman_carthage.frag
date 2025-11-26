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
in float v_layerNoise;
in float v_bendAmount;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_useTexture;
uniform float u_alpha;
uniform float u_time;
uniform float u_rainIntensity;
uniform int u_materialId;

out vec4 FragColor;

const vec3 k_leather_base = vec3(0.42, 0.30, 0.20);
const vec3 k_linen_base = vec3(0.88, 0.83, 0.74);
const vec3 k_bronze_base = vec3(0.58, 0.44, 0.20);
const float k_pi = 3.14159265;

float hash21(vec2 p) {
  p = fract(p * vec2(234.34, 435.345));
  p += dot(p, p + 34.45);
  return fract(p.x * p.y);
}

float noise3(vec3 p) {
  vec3 i = floor(p);
  vec3 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float n = i.x + i.y * 57.0 + i.z * 113.0;
  return mix(
      mix(mix(hash21(vec2(n, n + 1.0)), hash21(vec2(n + 57.0, n + 58.0)), f.x),
          mix(hash21(vec2(n + 113.0, n + 114.0)),
              hash21(vec2(n + 170.0, n + 171.0)), f.x),
          f.y),
      mix(mix(hash21(vec2(n + 226.0, n + 227.0)),
              hash21(vec2(n + 283.0, n + 284.0)), f.x),
          mix(hash21(vec2(n + 339.0, n + 340.0)),
              hash21(vec2(n + 396.0, n + 397.0)), f.x),
          f.y),
      f.z);
}

float fbm(vec3 p) {
  float value = 0.0;
  float amp = 0.5;
  float freq = 1.0;
  for (int i = 0; i < 5; ++i) {
    value += amp * noise3(p * freq);
    freq *= 1.9;
    amp *= 0.5;
  }
  return value;
}

vec3 hemi_ambient(vec3 n) {
  float up = clamp(n.y * 0.5 + 0.5, 0.0, 1.0);
  vec3 sky = vec3(0.58, 0.67, 0.78);
  vec3 ground = vec3(0.25, 0.21, 0.18);
  return mix(ground, sky, up);
}

float D_GGX(float NdotH, float a) {
  float a2 = a * a;
  float d = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
  return a2 / max(3.14159265 * d * d, 1e-6);
}

float geometry_schlick_ggx(float NdotX, float k) {
  return NdotX / max(NdotX * (1.0 - k) + k, 1e-6);
}

float geometry_smith(float NdotV, float NdotL, float roughness) {
  float r = roughness + 1.0;
  float k = (r * r) / 8.0;
  return geometry_schlick_ggx(NdotV, k) * geometry_schlick_ggx(NdotL, k);
}

vec3 fresnel_schlick(float cos_theta, vec3 F0) {
  return F0 + (1.0 - F0) * pow(1.0 - cos_theta, 5.0);
}

vec3 perturb(vec3 N, vec3 T, vec3 B, vec3 amount) {
  return normalize(N + T * amount.x + B * amount.y);
}

struct MaterialSample {
  vec3 color;
  vec3 normal;
  float roughness;
  vec3 F0;
};

MaterialSample sample_leather(vec3 base_color, vec3 pos, vec3 N, vec3 T,
                              vec3 B) {
  MaterialSample m;
  float grain = fbm(pos * 3.1 + vec3(v_layerNoise));
  float crease = fbm(pos * vec3(1.2, 4.0, 1.8));
  vec2 swell = vec2(grain - 0.5, crease - 0.5) * 0.15;
  vec3 Np = perturb(N, T, B, vec3(swell, v_bendAmount * 0.1));

  vec3 tint = mix(k_leather_base, base_color, 0.4);
  tint *= 1.0 - 0.18 * grain;
  tint += vec3(0.06) * smoothstep(0.4, 0.9, v_bodyHeight);
  tint = mix(tint, tint * vec3(0.7, 0.6, 0.5), smoothstep(0.65, 0.95, crease));

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.55 + grain * 0.3 - v_leatherTension * 0.2, 0.32, 0.95);
  m.F0 = vec3(0.035);
  return m;
}

MaterialSample sample_linen(vec3 base_color, vec3 pos, vec3 N, vec3 T, vec3 B) {
  MaterialSample m;
  float weave_u = sin(pos.x * 68.0);
  float weave_v = sin(pos.z * 72.0);
  float weft = weave_u * weave_v;
  float fray = fbm(pos * 5.0 + vec3(v_layerNoise));
  vec3 Np = perturb(N, T, B, vec3(weft * 0.05, fray * 0.04, 0.0));

  vec3 tint = mix(k_linen_base, base_color, 0.5);
  tint *= 1.0 - 0.12 * fray;
  tint = mix(tint, tint * vec3(0.92, 0.9, 0.85),
             smoothstep(0.5, 1.0, v_bodyHeight));

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.78 + fray * 0.25, 0.35, 0.95);
  m.F0 = vec3(0.028);
  return m;
}

MaterialSample sample_bronze(vec3 base_color, vec3 pos, vec3 N, vec3 T,
                             vec3 B) {
  MaterialSample m;
  float hammer = fbm(pos * 12.5);
  float patina = fbm(pos * 6.0 + vec3(3.1, 0.2, 5.5));
  vec3 Np =
      perturb(N, T, B, vec3((hammer - 0.5) * 0.12, (patina - 0.5) * 0.08, 0.0));

  vec3 tint = mix(k_bronze_base, base_color, 0.35);
  vec3 patina_color = vec3(0.22, 0.5, 0.42);
  tint = mix(tint, patina_color, clamp(patina * 0.55, 0.0, 0.6));
  tint += vec3(0.08) * pow(max(dot(Np, vec3(0.0, 1.0, 0.1)), 0.0), 6.0);

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.32 + hammer * 0.25 + patina * 0.2, 0.18, 0.72);
  m.F0 = mix(vec3(0.06), vec3(0.95, 0.68, 0.48), 0.85);
  return m;
}

vec3 crest_basis(vec3 n) {
  float az = atan(n.z, n.x);
  float el = acos(clamp(n.y, -1.0, 1.0));
  return vec3(az / (2.0 * k_pi), el / k_pi, n.y);
}

MaterialSample sample_carthage_helmet(vec3 base_color, vec3 pos, vec3 N, vec3 T,
                                      vec3 B) {
  MaterialSample m;

  // Base hammered bronze with wide-area patina streaks.
  float hammer = fbm(pos * 14.0 + vec3(v_layerNoise));
  float patina = fbm(pos * vec3(4.2, 6.0, 4.8) + vec3(1.3, 0.0, 2.1));
  vec3 Np =
      perturb(N, T, B, vec3((hammer - 0.5) * 0.10, (patina - 0.5) * 0.14, 0.0));

  // Radial meridian ribs anchored to surface direction.
  vec3 uv = crest_basis(N);
  float meridian = smoothstep(0.23, 0.0, abs(sin(uv.x * k_pi * 5.5)));
  float ridge = smoothstep(0.10, 0.0, abs(uv.x - 0.5));
  float crest = smoothstep(0.34, 0.16, uv.y) * ridge;
  float rim = smoothstep(0.80, 0.62, uv.y);
  float tip = smoothstep(0.82, 0.98, uv.y);
  vec3 rib_normal = vec3(0.0, crest * 0.42 + rim * 0.18, meridian * 0.26);

  // Brushed scratches running around the helmet circumference.
  float brush = sin(dot(T.xz, vec2(62.0, 54.0)) + uv.x * 18.0 + uv.y * 7.0);
  float scratch = smoothstep(0.6, 1.0, abs(brush));
  vec3 scratch_normal = T * (scratch * 0.10);

  Np = normalize(Np + T * rib_normal.z + B * rib_normal.y + scratch_normal);

  // Brow band mask based on height along the helmet (aligns to rim).
  float brow = smoothstep(0.18, 0.0, abs(uv.y - 0.70));
  float patina_mix = clamp(patina * 0.65 + brow * 0.3 + rim * 0.25, 0.0, 1.0);

  vec3 tint = mix(vec3(0.0, 1.0, 0.0), base_color, 0.15); // debug extreme green
  vec3 patina_color = vec3(0.26, 0.52, 0.40);
  vec3 crest_highlight = vec3(1.0, 0.92, 0.75);

  tint = mix(tint, patina_color, patina_mix * 0.7);
  tint += crest_highlight * crest * 0.65;
  tint = mix(tint, tint * vec3(1.06, 1.02, 0.96), rim * 0.35);
  tint = mix(tint, tint * vec3(1.12, 1.06, 1.00), tip * 0.45);
  tint += vec3(0.14) * brow;
  // Edge wear on rim and tip
  float edgeWear = clamp(rim * 0.6 + tip * 0.4, 0.0, 1.0);
  tint = mix(tint, tint * vec3(1.25, 1.18, 1.05), edgeWear);
  // Underside grime just below rim
  float underside = smoothstep(0.55, 0.28, uv.y);
  tint = mix(tint, tint * vec3(0.75, 0.70, 0.66), underside * 0.6);

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.22 + hammer * 0.26 + patina * 0.18 - crest * 0.12 -
                          rim * 0.06 - tip * 0.08,
                      0.10, 0.60);
  m.F0 = mix(vec3(0.08), vec3(0.96, 0.82, 0.56),
             clamp(0.48 + crest * 0.4 + brow * 0.18 + rim * 0.12 + tip * 0.16,
                   0.0, 1.0));
  return m;
}

vec3 apply_wet_darkening(vec3 color, float wet_mask) {
  return mix(color, color * 0.6, wet_mask);
}

void main() {
  vec3 base_color = u_color;
  if (u_useTexture) {
    base_color *= texture(u_texture, v_texCoord).rgb;
  }

  // Material ID: 0=body/skin, 1=armor, 2=helmet, 3=weapon, 4=shield
  bool is_armor = (u_materialId == 1);
  bool is_helmet = (u_materialId == 2);
  bool is_weapon = (u_materialId == 3);
  bool is_shield = (u_materialId == 4);

  // Use material IDs exclusively (no fallbacks)
  bool helmet_region = is_helmet;
  bool upper_region = is_armor;

  vec3 Nw = normalize(v_worldNormal);
  vec3 Tw = normalize(v_tangent);
  vec3 Bw = normalize(v_bitangent);

  MaterialSample mat;
  if (helmet_region) {
    mat = sample_carthage_helmet(base_color, v_worldPos, Nw, Tw, Bw);
  } else if (upper_region) {
    // Torso mixes linen and leather patches
    MaterialSample linen = sample_linen(base_color, v_worldPos, Nw, Tw, Bw);
    MaterialSample leather =
        sample_leather(base_color, v_worldPos * 1.3, Nw, Tw, Bw);
    float blend = smoothstep(0.3, 0.9, v_layerNoise);
    mat.color = mix(linen.color, leather.color, blend * 0.6);
    mat.normal = normalize(mix(linen.normal, leather.normal, blend * 0.5));
    mat.roughness = mix(linen.roughness, leather.roughness, blend);
    mat.F0 = mix(linen.F0, leather.F0, blend);
  } else {
    mat = sample_leather(base_color, v_worldPos * 1.1, Nw, Tw, Bw);
  }

  vec3 L = normalize(vec3(0.45, 1.15, 0.35));
  vec3 V = normalize(vec3(0.0, 0.0, 1.0));
  vec3 H = normalize(L + V);

  float NdotL = max(dot(mat.normal, L), 0.0);
  float NdotV = max(dot(mat.normal, V), 0.0);
  float NdotH = max(dot(mat.normal, H), 0.0);
  float VdotH = max(dot(V, H), 0.0);

  float wrap = helmet_region ? 0.18 : 0.34;
  float diff = max(NdotL * (1.0 - wrap) + wrap, 0.0);

  float a = max(0.01, mat.roughness * mat.roughness);
  float D = D_GGX(NdotH, a);
  float G = geometry_smith(NdotV, NdotL, mat.roughness);
  vec3 F = fresnel_schlick(VdotH, mat.F0);
  vec3 spec = (D * G * F) / max(4.0 * NdotL * NdotV + 1e-5, 1e-5);

  float kd = 1.0 - max(max(F.r, F.g), F.b);
  if (helmet_region) {
    kd *= 0.2;
  }

  float rain = clamp(u_rainIntensity, 0.0, 1.0);
  float wet_mask = rain * (1.0 - clamp(mat.normal.y, 0.0, 1.0)) *
                   (0.4 + 0.6 * fbm(v_worldPos * vec3(1.4, 0.8, 1.2)));

  vec3 ambient = hemi_ambient(mat.normal);
  vec3 color = apply_wet_darkening(mat.color, wet_mask);

  vec3 lighting =
      ambient * color * 0.55 + color * kd * diff + spec * max(NdotL, 0.0);

  float grime = fbm(v_worldPos * 2.6 + vec3(v_layerNoise, v_bendAmount, 0.0));
  lighting = mix(lighting, lighting * vec3(0.78, 0.74, 0.70),
                 smoothstep(0.5, 0.95, grime));

  FragColor = vec4(clamp(lighting, 0.0, 1.0), u_alpha);
}
