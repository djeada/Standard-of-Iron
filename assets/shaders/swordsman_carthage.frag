#version 330 core

in vec3 v_normal;
in vec3 v_worldNormal;
in vec3 v_tangent;
in vec3 v_bitangent;
in vec2 v_texCoord;
in vec3 v_worldPos;
in float v_armorLayer;
in float v_bodyHeight;
in float v_layerNoise;
in float v_plateStress;
in float v_lamellaPhase;
in float v_latitudeMix;
in float v_cuirassProfile;
in float v_chainmailMix;
in float v_frontMask;

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

// ---------------------------------------------------------------------------
// Material sampling
// ---------------------------------------------------------------------------
struct MaterialSample {
  vec3 color;
  vec3 normal;
  float roughness;
  vec3 F0;
};

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
  float hammer = fbm(pos * 14.0 + vec3(v_layerNoise));
  float patina = fbm(pos * 5.5 + vec3(2.1, 0.0, 4.2));
  float ridge = smoothstep(0.82, 1.02, v_bodyHeight);
  float rim = smoothstep(0.64, 0.86, v_bodyHeight) *
              (1.0 - smoothstep(0.94, 1.12, v_bodyHeight));
  vec3 Np =
      perturb(N, T, B, vec3((hammer - 0.5) * 0.10, (patina - 0.5) * 0.08, 0.0));

  vec3 tint = mix(vec3(0.62, 0.44, 0.18), base_color, 0.35);
  tint = mix(tint, vec3(0.26, 0.48, 0.38),
             clamp(patina * 0.65 + v_layerNoise * 0.2, 0.0, 0.8));
  tint += vec3(0.10) * ridge;
  tint += vec3(0.06) * rim;

  m.color = tint;
  m.normal = Np;
  m.roughness =
      clamp(0.24 + hammer * 0.22 + patina * 0.16 - rim * 0.08, 0.14, 0.70);
  m.F0 = mix(vec3(0.08), vec3(0.94, 0.82, 0.58),
             clamp(0.42 + patina * 0.25 + rim * 0.15, 0.0, 1.0));
  return m;
}

MaterialSample sample_bronze_cuirass(vec3 base_color, vec3 pos, vec3 N, vec3 T,
                                     vec3 B) {
  MaterialSample m;
  float hammer = fbm(pos * 11.0);
  float profile = v_cuirassProfile * 2.0 - 1.0;
  float front = v_frontMask;

  // Add stronger anatomical emboss + seam bevels
  float rib = sin(pos.x * 8.0 + profile * 3.2);
  float ab = sin(pos.y * 13.0 - profile * 3.8);
  float seam = smoothstep(0.42, 0.58, fract(pos.y * 4.5)) * 0.6;
  float edge = smoothstep(0.82, 0.96, v_bodyHeight) +
               smoothstep(0.18, 0.04, v_bodyHeight);

  vec3 Np = perturb(N, T, B,
                    vec3((rib + profile * 0.7) * 0.06 + seam * 0.05,
                         (ab + front * 0.45) * 0.05 + edge * 0.04, 0.0));

  // Force a stronger bronze anchor; palette only tints slightly.
  vec3 tint = mix(vec3(0.62, 0.46, 0.20), base_color, 0.20);
  vec3 patina_color = vec3(0.26, 0.48, 0.38);
  float patina = fbm(pos * 4.5 + vec3(1.6, 0.0, 2.3));
  tint = mix(tint, patina_color,
             clamp(patina * 0.55 + v_layerNoise * 0.2, 0.0, 0.65));
  tint += vec3(0.08) * front * smoothstep(0.45, 0.95, v_cuirassProfile);
  tint += vec3(0.07) * edge;
  tint -= vec3(0.05) * hammer;

  // Grime and cavity darkening toward the waist; edge brightening on ridges.
  float downward = smoothstep(0.35, 0.05, v_bodyHeight);
  float curvature = length(dFdx(N)) + length(dFdy(N));
  float edgeWear = smoothstep(0.12, 0.35, curvature);
  tint = mix(tint, tint * vec3(0.78, 0.72, 0.66), downward * 0.4);
  tint = mix(tint, tint * vec3(1.16, 1.10, 1.02), edgeWear * 0.6);

  m.color = tint;
  m.normal = Np;
  m.roughness =
      clamp(0.20 + hammer * 0.16 + patina * 0.08 - edgeWear * 0.12, 0.08, 0.60);
  m.F0 = mix(vec3(0.08), vec3(0.94, 0.72, 0.50),
             clamp(0.82 + edgeWear * 0.16, 0.0, 1.0));
  return m;
}

MaterialSample sample_chainmail(vec3 base_color, vec3 pos, vec3 N, vec3 T,
                                vec3 B) {
  MaterialSample m;
  vec2 uv = pos.xz * 14.0 + pos.yx * 5.5;
  float rings = chainmail_rings(uv * 0.55 + v_chainmailMix * 0.25);
  float weave = fbm(vec3(uv, 0.0) * 0.65 + v_layerNoise);
  float gaps = smoothstep(0.25, 0.55, weave);
  vec3 Np =
      perturb(N, T, B, vec3((rings - 0.5) * 0.22, (weave - 0.5) * 0.14, 0.0));

  vec3 tint = mix(vec3(0.52, 0.54, 0.60), base_color, 0.20);
  tint = mix(tint, tint * vec3(0.32, 0.28, 0.24),
             clamp(v_layerNoise * 0.45, 0.0, 0.8));
  tint += vec3(0.10) * rings;
  tint -= vec3(0.06) * gaps;

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.34 + rings * 0.14 + weave * 0.18, 0.18, 0.85);
  m.F0 = vec3(0.16, 0.16, 0.18);
  return m;
}

MaterialSample sample_linen(vec3 base_color, vec3 pos, vec3 N, vec3 T, vec3 B) {
  MaterialSample m;
  float warp = sin(pos.x * 120.0) * 0.04;
  float weft = sin(pos.z * 116.0) * 0.04;
  float slub = fbm(pos * 7.0) * 0.05;
  vec3 Np = normalize(N + T * (warp + slub) + B * (weft + slub * 0.5));

  vec3 tint = mix(vec3(0.88, 0.82, 0.72), base_color, 0.45);
  tint *= 1.0 - slub * 0.12;
  tint = mix(tint, tint * vec3(0.82, 0.76, 0.70),
             smoothstep(0.55, 1.0, v_bodyHeight));

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.62 + slub * 0.12, 0.35, 0.90);
  m.F0 = vec3(0.028);
  return m;
}

MaterialSample sample_leather(vec3 base_color, vec3 pos, vec3 N, vec3 T,
                              vec3 B) {
  MaterialSample m;
  float grain = fbm(pos * 6.0);
  float crack = fbm(pos * 11.0 + vec3(0.0, 1.7, 2.3));
  vec3 Np =
      perturb(N, T, B, vec3((grain - 0.5) * 0.10, (crack - 0.5) * 0.08, 0.0));

  vec3 tint = mix(vec3(0.38, 0.25, 0.15), base_color, 0.45);
  tint *= 1.0 - 0.10 * grain;
  tint += vec3(0.05) * smoothstep(0.4, 0.9, crack + v_layerNoise * 0.2);
  tint = mix(tint, tint * vec3(0.85, 0.80, 0.72),
             smoothstep(0.35, 0.15, v_bodyHeight)); // dustier toward ground

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
  float dent = fbm(pos * 12.0) * (0.4 + v_layerNoise * 0.3);
  vec3 Np = perturb(N, T, B, vec3((brushed - 0.5) * 0.06, dent * 0.05, 0.0));

  vec3 tint = mix(vec3(0.74, 0.76, 0.80), base_color, 0.45);
  tint += vec3(0.06) * brushed;
  tint -= vec3(0.08) * dent;

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.20 + brushed * 0.20 + dent * 0.18 - v_layerNoise * 0.08,
                      0.08, 0.80);
  m.F0 = vec3(0.62, 0.64, 0.66);
  return m;
}

MaterialSample sample_shield(vec3 base_color, vec3 pos, vec3 N, vec3 T,
                             vec3 B) {
  MaterialSample wood = sample_wood(base_color, pos * 0.9, N, T, B);
  MaterialSample boss =
      sample_steel(vec3(0.78, 0.74, 0.62), pos * 1.4, N, T, B);

  float boss_mask = smoothstep(0.12, 0.04, length(pos.xz));
  boss_mask = max(boss_mask, v_frontMask * 0.3);

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
    // Override palette to canonical materials so armor is visibly metal.
    vec3 bronze_base = vec3(0.62, 0.46, 0.20);
    vec3 steel_base = vec3(0.68, 0.70, 0.74);
    vec3 linen_base = vec3(0.86, 0.80, 0.72);
    vec3 leather_base = vec3(0.38, 0.25, 0.15);

    MaterialSample cuirass =
        sample_bronze_cuirass(bronze_base, v_worldPos, Nw, Tw, Bw);
    MaterialSample mail =
        sample_chainmail(steel_base, v_worldPos * 1.05, Nw, Tw, Bw);
    MaterialSample linen =
        sample_linen(linen_base, v_worldPos * 1.0, Nw, Tw, Bw);
    MaterialSample leather =
        sample_leather(leather_base, v_worldPos * 0.9, Nw, Tw, Bw);

    // Treat the entire armor piece as torso to avoid missing coverage.
    float torsoBand = 1.0;
    float skirtBand = 0.0;
    float mailBlend =
        clamp(smoothstep(0.15, 0.78, v_chainmailMix + v_layerNoise * 0.25),
              0.15, 1.0) *
        torsoBand;
    float cuirassBlend = torsoBand;
    float leatherBlend = skirtBand * 0.65;
    float linenBlend = skirtBand * 0.45;

    mat.color = cuirass.color;
    mat.color = mix(mat.color, mail.color, mailBlend);
    mat.color = mix(mat.color, linen.color, linenBlend);
    mat.color = mix(mat.color, leather.color, leatherBlend);
    // Make sure metal stays bright: bias toward bronze/steel luma.
    float armor_luma = dot(mat.color, vec3(0.299, 0.587, 0.114));
    mat.color =
        mix(mat.color, mat.color * 1.25, smoothstep(0.35, 0.65, armor_luma));

    mat.normal = cuirass.normal;
    mat.normal = normalize(mix(mat.normal, mail.normal, mailBlend));
    mat.normal = normalize(mix(mat.normal, linen.normal, linenBlend));
    mat.normal = normalize(mix(mat.normal, leather.normal, leatherBlend));

    mat.roughness = cuirass.roughness;
    mat.roughness = mix(mat.roughness, mail.roughness, mailBlend);
    mat.roughness = mix(mat.roughness, linen.roughness, linenBlend);
    mat.roughness = mix(mat.roughness, leather.roughness, leatherBlend);

    mat.F0 = cuirass.F0;
    mat.F0 = mix(mat.F0, mail.F0, mailBlend);
    mat.F0 = mix(mat.F0, linen.F0, linenBlend);
    mat.F0 = mix(mat.F0, leather.F0, leatherBlend);
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
  } else { // skin / cloth under layer
    mat = sample_skin(base_color, v_worldPos, Nw, Tw, Bw);
  }

  vec3 L = normalize(vec3(0.45, 1.12, 0.35));
  vec3 V = normalize(vec3(0.0, 0.0, 1.0));
  vec3 H = normalize(L + V);

  float NdotL = max(dot(mat.normal, L), 0.0);
  float NdotV = max(dot(mat.normal, V), 0.0);
  float NdotH = max(dot(mat.normal, H), 0.0);
  float VdotH = max(dot(V, H), 0.0);

  float wrap = is_helmet ? 0.15 : (is_armor ? 0.12 : 0.32);
  float diff = max(NdotL * (1.0 - wrap) + wrap, 0.10);

  float a = max(0.01, mat.roughness * mat.roughness);
  float D = D_GGX(NdotH, a);
  float G = geometry_smith(NdotV, NdotL, mat.roughness);
  vec3 F = fresnel_schlick(VdotH, mat.F0);
  vec3 spec = (D * G * F) / max(4.0 * NdotL * NdotV + 1e-5, 1e-5);

  float kd = 1.0 - max(max(F.r, F.g), F.b);
  if (is_helmet) {
    kd *= 0.25;
  }

  vec3 ambient = hemi_ambient(mat.normal);
  vec3 color = mat.color;

  float grime = fbm(v_worldPos * vec3(2.0, 1.4, 2.3));
  color =
      mix(color, color * vec3(0.78, 0.72, 0.68), smoothstep(0.45, 0.9, grime));

  vec3 lighting =
      ambient * color * 0.55 + color * kd * diff + spec * max(NdotL, 0.0);

  FragColor = vec4(saturate(lighting), u_alpha);
}
