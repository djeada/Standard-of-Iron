#version 330 core

in vec3 v_normal;
in vec3 v_worldNormal;
in vec3 v_tangent;
in vec3 v_bitangent;
in vec2 v_texCoord;
in vec3 v_worldPos;
in float v_armorLayer;
in float v_bodyHeight;
in float v_helmetDetail;
in float v_steelWear;
in float v_chainmailPhase;
in float v_rivetPattern;
in float v_leatherWear;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_useTexture;
uniform float u_alpha;
uniform int u_materialId;

out vec4 FragColor;

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------
const float k_pi = 3.14159265;

float saturate(float v) { return clamp(v, 0.0, 1.0); }
vec3 saturate(vec3 v) { return clamp(v, 0.0, 1.0); }

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
    freq *= 1.85;
    amp *= 0.55;
  }
  return value;
}

vec3 hemi_ambient(vec3 n) {
  float up = saturate(n.y * 0.5 + 0.5);
  vec3 sky = vec3(0.54, 0.68, 0.82);
  vec3 ground = vec3(0.32, 0.26, 0.20);
  return mix(ground, sky, up);
}

float D_GGX(float NdotH, float a) {
  float a2 = a * a;
  float d = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
  return a2 / max(k_pi * d * d, 1e-6);
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

float chainmail_rings(vec2 p) {
  vec2 uv = p * 32.0;

  vec2 g0 = fract(uv) - 0.5;
  float r0 = length(g0);
  float fw0 = fwidth(r0) * 1.2;
  float ring0 = smoothstep(0.30 + fw0, 0.30 - fw0, r0) -
                smoothstep(0.20 + fw0, 0.20 - fw0, r0);

  vec2 g1 = fract(uv + vec2(0.5, 0.0)) - 0.5;
  float r1 = length(g1);
  float fw1 = fwidth(r1) * 1.2;
  float ring1 = smoothstep(0.30 + fw1, 0.30 - fw1, r1) -
                smoothstep(0.20 + fw1, 0.20 - fw1, r1);

  return (ring0 + ring1) * 0.15;
}

struct MaterialSample {
  vec3 color;
  vec3 normal;
  float roughness;
  vec3 F0;
};

// ---------------------------------------------------------------------------
// Material sampling
// ---------------------------------------------------------------------------
MaterialSample sample_skin(vec3 base_color, vec3 pos, vec3 N, vec3 T, vec3 B) {
  MaterialSample m;
  float pores = fbm(pos * 12.0);
  float freckles = smoothstep(0.55, 0.78, fbm(pos * 6.5 + vec3(1.7, 0.0, 0.3)));
  vec3 Np = perturb(N, T, B,
                    vec3((pores - 0.5) * 0.05, (freckles - 0.5) * 0.04, 0.0));

  vec3 tint = mix(base_color, vec3(0.93, 0.80, 0.68), 0.35);
  tint += vec3(0.03) * freckles;

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.48 + pores * 0.10, 0.32, 0.72);
  m.F0 = vec3(0.028);
  return m;
}

MaterialSample sample_bronze_helmet(vec3 base_color, vec3 pos, vec3 N, vec3 T,
                                    vec3 B) {
  MaterialSample m;
  float hammer = fbm(pos * 14.0);
  float patina = fbm(pos * 5.5 + vec3(2.1, 0.0, 4.2));
  float band = v_helmetDetail;
  float rivet = v_rivetPattern;
  vec3 Np =
      perturb(N, T, B, vec3((hammer - 0.5) * 0.10, (patina - 0.5) * 0.06, 0.0));

  vec3 tint = mix(vec3(0.62, 0.44, 0.18), base_color, 0.35);
  tint = mix(tint, vec3(0.26, 0.48, 0.38),
             clamp(patina * 0.65 + v_steelWear * 0.3, 0.0, 0.8));
  tint += vec3(0.12) * band;
  tint += vec3(0.05) * rivet;

  m.color = tint;
  m.normal = Np;
  m.roughness =
      clamp(0.26 + hammer * 0.22 + patina * 0.16 - band * 0.08, 0.14, 0.70);
  m.F0 = mix(vec3(0.08), vec3(0.94, 0.82, 0.58),
             clamp(0.45 + patina * 0.25 + band * 0.20, 0.0, 1.0));
  return m;
}

MaterialSample sample_linen(vec3 base_color, vec3 pos, vec3 N, vec3 T, vec3 B) {
  MaterialSample m;
  float warp = sin(pos.x * 120.0) * 0.05;
  float weft = sin(pos.z * 116.0) * 0.05;
  float slub = fbm(pos * 7.5) * 0.05;
  vec3 Np = normalize(N + T * (warp + slub) + B * (weft + slub * 0.5));

  vec3 tint = mix(vec3(0.88, 0.82, 0.72), base_color, 0.45);
  tint *= 1.0 - slub * 0.15;
  tint = mix(tint, tint * vec3(0.82, 0.76, 0.70),
             smoothstep(0.55, 1.0, v_bodyHeight));

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.62 + slub * 0.12, 0.35, 0.90);
  m.F0 = vec3(0.028);
  return m;
}

MaterialSample sample_bronze_scales(vec3 base_color, vec3 pos, vec3 N, vec3 T,
                                    vec3 B) {
  MaterialSample m;
  float scallop = smoothstep(0.35, 0.05, fract(pos.y * 6.4 + v_chainmailPhase));
  float row = smoothstep(0.75, 0.98, sin(pos.y * 9.0 + pos.x * 1.5));
  float hammer = fbm(pos * 12.0 + vec3(0.0, 1.8, 2.4));
  vec3 Np = perturb(N, T, B,
                    vec3((hammer - 0.5) * 0.08, (scallop - 0.5) * 0.05, 0.0));

  vec3 tint = mix(vec3(0.64, 0.46, 0.20), base_color, 0.40);
  tint += vec3(0.10) * scallop;
  tint = mix(tint, tint * vec3(0.26, 0.48, 0.38),
             clamp(v_steelWear * 0.55, 0.0, 0.6));
  tint += vec3(0.05) * row;

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.34 + hammer * 0.20 - scallop * 0.08, 0.16, 0.78);
  m.F0 = vec3(0.12, 0.10, 0.07);
  return m;
}

MaterialSample sample_chainmail(vec3 base_color, vec3 pos, vec3 N, vec3 T,
                                vec3 B) {
  MaterialSample m;
  vec2 mail_uv = pos.xz * 1.2;
  float rings = chainmail_rings(mail_uv);
  float grain = fbm(pos * 15.0 + vec3(1.7));
  vec3 Np =
      perturb(N, T, B, vec3((rings - 0.5) * 0.20, (grain - 0.5) * 0.10, 0.0));

  vec3 tint = mix(vec3(0.62, 0.64, 0.68), base_color, 0.35);
  tint = mix(tint, tint * vec3(0.34, 0.30, 0.26),
             clamp(v_steelWear * 0.6, 0.0, 0.7));
  tint += vec3(0.10) * rings;

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.28 + rings * 0.14 + grain * 0.12, 0.16, 0.82);
  m.F0 = vec3(0.16, 0.16, 0.18);
  return m;
}

MaterialSample sample_leather(vec3 base_color, vec3 pos, vec3 N, vec3 T,
                              vec3 B) {
  MaterialSample m;
  float grain = fbm(pos * 6.0);
  float crack = fbm(pos * 11.0 + vec3(0.0, 1.7, 2.3));
  float wear = v_leatherWear;
  vec3 Np =
      perturb(N, T, B, vec3((grain - 0.5) * 0.10, (crack - 0.5) * 0.08, 0.0));

  vec3 tint = mix(vec3(0.38, 0.25, 0.15), base_color, 0.45);
  tint *= 1.0 - 0.10 * grain;
  tint += vec3(0.05) * smoothstep(0.4, 0.9, crack + wear * 0.2);

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.55 + grain * 0.22 - crack * 0.12, 0.25, 0.95);
  m.F0 = vec3(0.035);
  return m;
}

MaterialSample sample_wood(vec3 base_color, vec3 pos, vec3 N, vec3 T, vec3 B) {
  MaterialSample m;
  float grain = fbm(pos * 10.0);
  float rings = sin(pos.y * 24.0 + pos.x * 3.0);
  vec3 Np = perturb(N, T, B, vec3(grain * 0.06, rings * 0.04, 0.0));

  vec3 tint = mix(vec3(0.42, 0.32, 0.20), base_color, 0.45);
  tint *= 1.0 + grain * 0.10 + rings * 0.04;

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.52 + grain * 0.18, 0.28, 0.90);
  m.F0 = vec3(0.028);
  return m;
}

MaterialSample sample_steel(vec3 base_color, vec3 pos, vec3 N, vec3 T, vec3 B) {
  MaterialSample m;
  float brushed = abs(sin(pos.y * 90.0)) * 0.04;
  float dent = fbm(pos * 12.0) * v_steelWear;
  vec3 Np = perturb(N, T, B, vec3((brushed - 0.5) * 0.06, dent * 0.05, 0.0));

  vec3 tint = mix(vec3(0.74, 0.76, 0.80), base_color, 0.45);
  tint += vec3(0.06) * brushed;
  tint -= vec3(0.08) * dent;

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.20 + brushed * 0.20 + dent * 0.18 - v_steelWear * 0.08,
                      0.08, 0.80);
  m.F0 = vec3(0.62, 0.64, 0.66);
  return m;
}

MaterialSample sample_shield(vec3 base_color, vec3 pos, vec3 N, vec3 T,
                             vec3 B) {
  MaterialSample wood = sample_wood(base_color, pos * 0.9, N, T, B);
  MaterialSample boss =
      sample_steel(vec3(0.78, 0.74, 0.62), pos * 1.4, N, T, B);

  float boss_mask = smoothstep(0.08, 0.02, length(pos.xz));
  boss_mask = max(boss_mask, v_rivetPattern * 0.25);

  MaterialSample m;
  m.color = mix(wood.color, boss.color, boss_mask);
  m.normal = normalize(mix(wood.normal, boss.normal, boss_mask));
  m.roughness = mix(wood.roughness, boss.roughness, boss_mask);
  m.F0 = mix(wood.F0, boss.F0, boss_mask);
  return m;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
void main() {
  vec3 base_color = u_color;
  if (u_useTexture) {
    base_color *= texture(u_texture, v_texCoord).rgb;
  }

  // Material ID: 0=body/skin, 1=armor, 2=helmet, 3=weapon, 4=shield
  bool is_skin = (u_materialId == 0);
  bool is_armor = (u_materialId == 1);
  bool is_helmet = (u_materialId == 2);
  bool is_weapon = (u_materialId == 3);
  bool is_shield = (u_materialId == 4);

  vec3 Nw = normalize(v_worldNormal);
  vec3 Tw = normalize(v_tangent);
  vec3 Bw = normalize(v_bitangent);

  MaterialSample mat;
  if (is_helmet) {
    mat = sample_bronze_helmet(base_color, v_worldPos, Nw, Tw, Bw);
  } else if (is_armor) {
    vec3 leather_base = vec3(0.44, 0.30, 0.19);
    vec3 linen_base = vec3(0.86, 0.80, 0.72);
    vec3 bronze_base = vec3(0.62, 0.46, 0.20);
    vec3 chain_base = vec3(0.78, 0.80, 0.82);

    MaterialSample leather =
        sample_leather(leather_base, v_worldPos * 0.85, Nw, Tw, Bw);
    MaterialSample linen =
        sample_linen(linen_base, v_worldPos * 1.0, Nw, Tw, Bw);
    MaterialSample scales =
        sample_bronze_scales(bronze_base, v_worldPos * 0.95, Nw, Tw, Bw);
    MaterialSample mail =
        sample_chainmail(chain_base, v_worldPos * 0.9, Nw, Tw, Bw);

    // Treat the entire armor piece as torso to avoid losing coverage to height
    // band thresholds.
    float torsoBand = 1.0;
    float skirtBand = 0.0;
    float mailBlend =
        clamp(smoothstep(0.25, 0.85, v_chainmailPhase + v_steelWear * 0.35),
              0.0, 1.0) *
        torsoBand * 0.30;
    float scaleBlend =
        clamp(0.28 + v_steelWear * 0.55, 0.0, 1.0) * torsoBand * 0.55;
    float linenBlend = skirtBand * 0.40;
    float leatherOverlay = skirtBand * 0.90 + torsoBand * 0.30;

    // subtle edge tint to lift highlights
    float edge = 1.0 - clamp(dot(Nw, vec3(0.0, 1.0, 0.0)), 0.0, 1.0);
    vec3 highlight = vec3(0.10, 0.08, 0.05) * smoothstep(0.3, 0.9, edge);

    // Leather-first blend with lighter linen skirt and subtle bronze/chain
    mat.color = leather.color;
    mat.color = mix(mat.color, linen.color, linenBlend);
    mat.color = mix(mat.color, scales.color, scaleBlend);
    mat.color = mix(mat.color, mail.color, mailBlend);
    mat.color = mix(mat.color, leather.color + highlight, leatherOverlay);

    float leather_depth = clamp(
        leatherOverlay * 0.8 + linenBlend * 0.2 + scaleBlend * 0.15, 0.0, 1.0);
    mat.color = mix(mat.color, mat.color * 0.88 + vec3(0.04, 0.03, 0.02),
                    leather_depth * 0.35);

    mat.normal = leather.normal;
    mat.normal = normalize(mix(mat.normal, linen.normal, linenBlend));
    mat.normal = normalize(mix(mat.normal, scales.normal, scaleBlend));
    mat.normal = normalize(mix(mat.normal, mail.normal, mailBlend));

    mat.roughness = leather.roughness;
    mat.roughness = mix(mat.roughness, linen.roughness, linenBlend);
    mat.roughness = mix(mat.roughness, scales.roughness, scaleBlend);
    mat.roughness = mix(mat.roughness, mail.roughness, mailBlend);

    mat.F0 = leather.F0;
    mat.F0 = mix(mat.F0, linen.F0, linenBlend);
    mat.F0 = mix(mat.F0, scales.F0, scaleBlend);
    mat.F0 = mix(mat.F0, mail.F0, mailBlend);
  } else if (is_weapon) {
    if (v_bodyHeight > 0.55) {
      mat = sample_steel(base_color, v_worldPos * 1.4, Nw, Tw, Bw);
    } else if (v_bodyHeight > 0.25) {
      mat = sample_wood(base_color, v_worldPos * 0.9, Nw, Tw, Bw);
    } else {
      mat = sample_leather(base_color, v_worldPos * 1.0, Nw, Tw, Bw);
    }
  } else if (is_shield) {
    mat = sample_shield(base_color, v_worldPos, Nw, Tw, Bw);
  } else { // skin / clothing
    mat = sample_skin(base_color, v_worldPos, Nw, Tw, Bw);
  }

  vec3 L = normalize(vec3(0.45, 1.12, 0.35));
  vec3 V = normalize(vec3(0.0, 0.0, 1.0));
  vec3 H = normalize(L + V);

  float NdotL = max(dot(mat.normal, L), 0.0);
  float NdotV = max(dot(mat.normal, V), 0.0);
  float NdotH = max(dot(mat.normal, H), 0.0);
  float VdotH = max(dot(V, H), 0.0);

  float wrap = is_helmet ? 0.15 : (is_armor ? 0.26 : 0.32);
  float diff = max(NdotL * (1.0 - wrap) + wrap, 0.18);

  float a = max(0.01, mat.roughness * mat.roughness);
  float D = D_GGX(NdotH, a);
  float G = geometry_smith(NdotV, NdotL, mat.roughness);
  vec3 F = fresnel_schlick(VdotH, mat.F0);
  vec3 spec = (D * G * F) / max(4.0 * NdotL * NdotV + 1e-5, 1e-5);

  float kd = 1.0 - max(max(F.r, F.g), F.b);
  if (is_helmet)
    kd *= 0.25;

  vec3 ambient = hemi_ambient(mat.normal);
  vec3 color = mat.color;

  // Dust and campaign wear
  float grime = fbm(v_worldPos * vec3(2.0, 1.4, 2.3));
  color =
      mix(color, color * vec3(0.78, 0.72, 0.68), smoothstep(0.45, 0.9, grime));

  vec3 lighting =
      ambient * color * 0.55 + color * kd * diff + spec * max(NdotL, 0.0);

  FragColor = vec4(saturate(lighting), u_alpha);
}
