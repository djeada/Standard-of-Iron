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

float triplanarNoise(vec3 pos, vec3 normal, float scale) {
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
    total += amp * triplanarNoise(pos * freq, normal, scale * freq);
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

float colorDistance(vec3 a, vec3 b) { return length(a - b); }

// ----------------------------- Geometry helpers -----------------------------
float fresnelSchlick(float cosTheta, float F0) {
  return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 fresnelSchlick(vec3 F0, float cosTheta) {
  return F0 + (vec3(1.0) - F0) * pow(1.0 - cosTheta, 5.0);
}

float computeCurvature(vec3 normal) {
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
  float gV = NdotV / (NdotV * (1.0 - k) + k);
  float gL = NdotL / (NdotL * (1.0 - k) + k);
  return gV * gL;
}

// Perturb normal procedurally to add micro detail per material
vec3 perturbNormalLeather(vec3 N, vec3 T, vec3 B, vec3 P) {
  float g1 = fbm(P * 6.0, N, 8.0);
  float g2 = fbm(P * 18.0 + vec3(2.1), N, 24.0);
  vec3 nT = T * (g1 * 0.06 + g2 * 0.02);
  vec3 nB = B * (g1 * 0.03 + g2 * 0.03);
  vec3 pN = normalize(N + nT + nB);
  return pN;
}

vec3 perturbNormalLinen(vec3 N, vec3 T, vec3 B, vec3 P) {
  // warp/weft weave
  float warp = sin(P.x * 140.0) * 0.06;
  float weft = sin(P.z * 146.0) * 0.06;
  float slub = fbm(P * 7.0, N, 10.0) * 0.04;
  vec3 pN = normalize(N + T * (warp + slub) + B * (weft + slub * 0.5));
  return pN;
}

vec3 perturbNormalBronze(vec3 N, vec3 T, vec3 B, vec3 P) {
  float hammer = fbm(P * 16.0, N, 22.0) * 0.10;
  float ripple = fbm(P * 40.0 + vec3(3.7), N, 55.0) * 0.03;
  vec3 pN = normalize(N + T * hammer + B * ripple);
  return pN;
}

// Clearcoat (water film) lobe for rain
vec3 clearcoatSpec(vec3 N, vec3 L, vec3 V, float coatStrength,
                   float coatRough) {
  vec3 H = normalize(L + V);
  float NdotV = max(dot(N, V), 0.0);
  float NdotL = max(dot(N, L), 0.0);
  float NdotH = max(dot(N, H), 0.0);
  float a = max(coatRough, 0.02);
  float D = D_GGX(NdotH, a);
  float G = G_Smith(NdotV, NdotL, a);
  // IOR ~1.33 → F0 ≈ 0.02
  float F = fresnelSchlick(max(dot(H, V), 0.0), 0.02);
  float spec = (D * G * F) / max(4.0 * NdotV * NdotL + 1e-5, 1e-5);
  return vec3(spec * coatStrength);
}

mat3 makeTangentBasis(vec3 N, vec3 T, vec3 B) {
  vec3 t = normalize(T - N * dot(N, T));
  vec3 b = normalize(B - N * dot(N, B));
  if (length(t) < 1e-4)
    t = vec3(1.0, 0.0, 0.0);
  if (length(b) < 1e-4)
    b = normalize(cross(N, t));
  return mat3(t, b, N);
}

vec3 applyMicroNormal(vec3 baseN, vec3 T, vec3 B, vec3 pos, float intensity) {
  mat3 basis = makeTangentBasis(baseN, T, B);
  float noiseX = fbm(pos * 18.0 + vec3(1.37, 2.07, 3.11), baseN, 24.0);
  float noiseY = fbm(pos * 20.0 + vec3(-2.21, 1.91, 0.77), baseN, 26.0);
  vec3 tangentNormal =
      normalize(vec3(noiseX * 2.0 - 1.0, noiseY * 2.0 - 1.0, 1.0));
  tangentNormal.xy *= intensity;
  return normalize(basis * tangentNormal);
}

vec3 toneMapAndGamma(vec3 color) {
  color = color / (color + vec3(1.0));
  return pow(color, vec3(1.0 / 2.2));
}

vec3 computeAmbient(vec3 normal) {
  float up = clamp(normal.y, 0.0, 1.0);
  float down = clamp(-normal.y, 0.0, 1.0);
  vec3 sky = vec3(0.60, 0.70, 0.85);
  vec3 ground = vec3(0.40, 0.34, 0.28);
  return sky * (0.25 + 0.55 * up) + ground * (0.10 + 0.35 * down);
}

MaterialSample makeLeatherSample(
    vec3 baseColor, vec3 Nw, vec3 Tw, vec3 Bw, vec3 worldPos, float tension,
    float bodyHeight, float layer, float wetMask, float curvature) {
  MaterialSample mat;
  mat.metallic = 0.0;

  vec3 targetLeather = vec3(0.42, 0.30, 0.20);
  vec3 color = mix(baseColor, targetLeather, 0.12);

  float torsoBleach = mix(0.92, 1.08, clamp(bodyHeight, 0.0, 1.0));
  color *= vec3(torsoBleach, mix(0.90, 0.98, bodyHeight),
                mix(0.87, 0.95, bodyHeight));

  float macro = fbm(worldPos * 3.2, Nw, 4.2);
  float medium = fbm(worldPos * 7.6, Nw, 8.5);
  float fine = fbm(worldPos * 16.0, Nw, 18.0);
  float pores = fbm(worldPos * 32.0 + vec3(3.7), Nw, 38.0);
  float albedoNoise = macro * 0.35 + medium * 0.30 + fine * 0.22 + pores * 0.13;
  color *= mix(0.88, 1.12, albedoNoise);

  float dirt = fbm(worldPos * vec3(2.5, 1.1, 2.5), Nw, 3.5) *
               (1.0 - clamp(bodyHeight, 0.0, 1.0));
  color = mix(color, color * vec3(0.70, 0.58, 0.42),
              smoothstep(0.45, 0.75, dirt) * 0.25);

  float salt =
      smoothstep(0.62, 0.95, fbm(worldPos * vec3(12.0, 6.0, 12.0), Nw, 14.0));
  color = mix(color, color * vec3(0.82, 0.80, 0.76), salt * 0.18);

  float strapBand = smoothstep(0.1, 0.9, sin(worldPos.y * 5.2 + layer * 1.7));
  float seam = sin(worldPos.x * 3.7 + worldPos.z * 2.9);
  color = mix(color, color * vec3(0.86, 0.83, 0.78), strapBand * 0.12);
  color = mix(color, baseColor, smoothstep(0.2, 0.8, seam) * 0.08);

  color = mix(color, color * 0.55, wetMask * 0.85);

  vec3 macroNormal = perturbNormalLeather(Nw, Tw, Bw, worldPos);
  vec3 normal = applyMicroNormal(macroNormal, Tw, Bw, worldPos, 0.55);

  float grain = macro * 0.40 + medium * 0.32 + fine * 0.20 + pores * 0.08;
  float roughBase = clamp(0.78 - tension * 0.18 + grain * 0.15, 0.52, 0.92);
  float wetInfluence = mix(0.0, -0.28, wetMask);
  mat.roughness = clamp(roughBase + wetInfluence, 0.35, 0.95);

  float crease =
      smoothstep(0.45, 0.75, fbm(worldPos * vec3(1.4, 3.5, 1.4), Nw, 2.6));
  float layerAO = mix(0.85, 0.65, clamp(layer / 2.0, 0.0, 1.0));
  float curvatureAO = mix(0.75, 1.0, clamp(1.0 - curvature * 1.2, 0.0, 1.0));
  mat.ao = clamp((1.0 - crease * 0.35) * layerAO * curvatureAO *
                     (0.9 - wetMask * 0.15),
                 0.35, 1.0);

  mat.albedo = color;
  mat.normal = normal;
  vec3 tintSpec = mix(vec3(0.035), color, 0.16);
  mat.F0 = mix(tintSpec, vec3(0.08), wetMask * 0.45);
  return mat;
}

MaterialSample makeLinenSample(vec3 baseColor, vec3 Nw, vec3 Tw, vec3 Bw,
                               vec3 worldPos, float bodyHeight, float wetMask,
                               float curvature) {
  MaterialSample mat;
  mat.metallic = 0.0;

  vec3 target = vec3(0.88, 0.85, 0.78);
  vec3 color = mix(target, baseColor, 0.45);
  float weave = sin(worldPos.x * 62.0) * sin(worldPos.z * 66.0) * 0.08;
  float sizing = fbm(worldPos * 3.0, Nw, 4.5) * 0.10;
  float fray = fbm(worldPos * 9.0, Nw, 10.0) *
               clamp(1.4 - clamp(Nw.y, 0.0, 1.0), 0.0, 1.0) * 0.12;
  color += vec3(weave * 0.35);
  color -= vec3(sizing * 0.5);
  color -= vec3(fray * 0.12);

  float dust = clamp(1.0 - Nw.y, 0.0, 1.0) * fbm(worldPos * 1.1, Nw, 2.0);
  float sweat =
      smoothstep(0.6, 1.0, bodyHeight) * fbm(worldPos * 2.4, Nw, 3.1);
  color = mix(color, color * (1.0 - dust * 0.35), 0.7);
  color = mix(color, color * vec3(0.96, 0.93, 0.88),
              1.0 - clamp(sweat * 0.5, 0.0, 1.0));

  color *= (1.0 - wetMask * 0.35);

  mat.albedo = color;
  mat.normal = perturbNormalLinen(Nw, Tw, Bw, worldPos);
  float roughNoise = fbm(worldPos * 5.0, Nw, 7.5);
  mat.roughness =
      clamp(0.82 + roughNoise * 0.12 - wetMask * 0.22, 0.55, 0.96);
  mat.ao = clamp(0.85 - dust * 0.20 - sweat * 0.15 + curvature * 0.05, 0.4,
                 1.0);
  mat.F0 = vec3(0.028);
  return mat;
}

MaterialSample makeBronzeSample(vec3 baseColor, vec3 Nw, vec3 Tw, vec3 Bw,
                                vec3 worldPos, float wetMask, float curvature) {
  MaterialSample mat;
  mat.metallic = 0.9;
  vec3 bronzeWarm = vec3(0.58, 0.44, 0.20);
  vec3 cuprite = vec3(0.36, 0.18, 0.10);
  vec3 malachite = vec3(0.18, 0.45, 0.36);

  vec3 macroNormal = perturbNormalBronze(Nw, Tw, Bw, worldPos);
  float hammer = fbm(worldPos * 14.0, Nw, 20.0) * 0.18;
  float patina = fbm(worldPos * 6.0 + vec3(5.0), Nw, 8.0) * 0.14;
  float runOff = fbm(worldPos * vec3(1.2, 3.4, 1.2), Nw, 2.2) *
                 (1.0 - clamp(Nw.y, 0.0, 1.0));

  vec3 bronzeBase = mix(bronzeWarm, baseColor, 0.35) + vec3(hammer);
  vec3 withCuprite =
      mix(bronzeBase, cuprite,
          smoothstep(0.70, 0.95, fbm(worldPos * 9.0, Nw, 12.0)));
  vec3 color = mix(withCuprite, malachite,
                   clamp(patina * 0.5 + runOff * 0.6, 0.0, 1.0));

  color = mix(color, color * 0.65, wetMask * 0.6);

  mat.albedo = color;
  mat.normal = applyMicroNormal(macroNormal, Tw, Bw, worldPos, 0.35);
  mat.roughness =
      clamp(0.32 + hammer * 0.25 + patina * 0.15 + wetMask * -0.18, 0.18, 0.75);
  mat.ao = clamp(0.85 - patina * 0.3 + curvature * 0.1, 0.45, 1.0);
  vec3 F0 = mix(vec3(0.06), clamp(bronzeWarm, 0.0, 1.0), mat.metallic);
  mat.F0 = mix(F0, vec3(0.08), wetMask * 0.3);
  return mat;
}

MaterialSample makeFallbackSample(vec3 baseColor, vec3 Nw, vec3 Tw, vec3 Bw,
                                  vec3 worldPos, float wetMask,
                                  float curvature) {
  MaterialSample mat;
  mat.albedo = baseColor * (0.9 + 0.2 * fbm(worldPos * 4.0, Nw, 5.5));
  vec3 macroNormal = perturbNormalLeather(Nw, Tw, Bw, worldPos);
  mat.normal = applyMicroNormal(macroNormal, Tw, Bw, worldPos, 0.25);
  mat.roughness = clamp(0.60 - wetMask * 0.20, 0.35, 0.85);
  mat.ao = clamp(0.8 + curvature * 0.1, 0.4, 1.0);
  mat.metallic = 0.0;
  mat.F0 = vec3(0.04);
  mat.albedo = mix(mat.albedo, mat.albedo * 0.7, wetMask * 0.5);
  return mat;
}

MaterialSample makeWoodSample(vec3 baseColor, vec3 Nw, vec3 Tw, vec3 Bw,
                              vec3 worldPos, float wetMask, float curvature) {
  MaterialSample mat;
  vec3 color = baseColor;
  float grain = sin(worldPos.y * 18.0 + fbm(worldPos * 2.0, Nw, 2.5) * 3.5);
  float rings = sin(worldPos.x * 6.5 + grain * 2.0);
  float burn = fbm(worldPos * vec3(1.2, 0.6, 1.2), Nw, 1.6);
  color *= 1.0 + grain * 0.05;
  color -= burn * 0.08;
  color = mix(color, color * 0.6, wetMask * 0.4);
  mat.albedo = color;
  vec3 macroNormal = applyMicroNormal(Nw, Tw, Bw, worldPos, 0.18);
  mat.normal = normalize(macroNormal + Tw * (grain * 0.05));
  mat.roughness =
      clamp(0.62 + fbm(worldPos * 6.0, Nw, 6.0) * 0.15 - wetMask * 0.18, 0.35,
            0.92);
  mat.ao = clamp(0.9 - burn * 0.15 + curvature * 0.08, 0.4, 1.0);
  mat.metallic = 0.0;
  mat.F0 = vec3(0.035);
  return mat;
}

MaterialSample makeSkinSample(vec3 baseColor, vec3 Nw, vec3 Tw, vec3 Bw,
                              vec3 worldPos, float wetMask, float curvature) {
  MaterialSample mat;
  vec3 color = baseColor;
  float freckle = fbm(worldPos * vec3(9.0, 4.0, 9.0), Nw, 12.0);
  float blush = smoothstep(0.2, 0.9, Nw.y) * 0.08;
  color += freckle * 0.03;
  color += blush;
  color = mix(color, color * 0.9, wetMask * 0.15);
  mat.albedo = color;
  mat.normal = applyMicroNormal(Nw, Tw, Bw, worldPos, 0.12);
  mat.roughness = clamp(0.58 + freckle * 0.1 - wetMask * 0.15, 0.38, 0.85);
  mat.ao = clamp(0.92 - curvature * 0.15, 0.5, 1.0);
  mat.metallic = 0.0;
  mat.F0 = vec3(0.028);
  return mat;
}

MaterialSample makeHairSample(vec3 baseColor, vec3 Nw, vec3 Tw, vec3 Bw,
                              vec3 worldPos, float wetMask) {
  MaterialSample mat;
  vec3 color = baseColor * (0.9 + fbm(worldPos * 5.0, Nw, 7.0) * 0.12);
  float strand = sin(worldPos.x * 64.0) * 0.08;
  color += strand * 0.04;
  color = mix(color, color * 0.7, wetMask * 0.35);
  mat.albedo = color;
  mat.normal = applyMicroNormal(Nw, Tw, Bw, worldPos, 0.45);
  mat.roughness = clamp(0.42 + strand * 0.05 - wetMask * 0.18, 0.2, 0.7);
  mat.ao = 0.8;
  mat.metallic = 0.0;
  mat.F0 = vec3(0.06);
  return mat;
}

MaterialSample makeClothSample(vec3 baseColor, vec3 Nw, vec3 Tw, vec3 Bw,
                               vec3 worldPos, float wetMask, float curvature) {
  MaterialSample mat;
  vec3 color = baseColor;
  float weave = sin(worldPos.x * 52.0) * sin(worldPos.z * 55.0) * 0.08;
  float fold = fbm(worldPos * 3.4, Nw, 4.0) * 0.15;
  float stripe = sin(worldPos.y * 8.0 + worldPos.x * 2.0);
  float teamAccent = smoothstep(0.2, 0.9, stripe);
  color += weave * 0.3;
  color += fold * 0.08;
  color = mix(color, color * 1.15, teamAccent * 0.08);
  color = mix(color, color * 0.7, wetMask * 0.3);
  mat.albedo = color;
  mat.normal = applyMicroNormal(Nw, Tw, Bw, worldPos, 0.12);
  mat.roughness =
      clamp(0.78 + weave * 0.1 + fold * 0.05 - wetMask * 0.2, 0.45, 0.95);
  mat.ao = clamp(0.9 - fold * 0.2 + curvature * 0.05, 0.4, 1.0);
  mat.metallic = 0.0;
  mat.F0 = vec3(0.03);
  return mat;
}

MaterialSample makeMetalSample(vec3 baseColor, vec3 Nw, vec3 Tw, vec3 Bw,
                               vec3 worldPos, float wetMask, float curvature) {
  MaterialSample mat;
  mat.metallic = 1.0;
  vec3 silver = vec3(0.78, 0.80, 0.88);
  vec3 color = mix(silver, baseColor, 0.25);
  float hammer = fbm(worldPos * 22.0, Nw, 28.0) * 0.08;
  float scrape = fbm(worldPos * 8.0, Nw, 10.0) * 0.12;
  color += hammer * 0.2;
  color -= scrape * 0.08;
  color = mix(color, color * 0.7, wetMask * 0.4);
  mat.albedo = color;
  mat.normal = applyMicroNormal(Nw, Tw, Bw, worldPos, 0.25);
  mat.roughness =
      clamp(0.22 + hammer * 0.12 + scrape * 0.08 - wetMask * 0.15, 0.08, 0.5);
  mat.ao = clamp(0.85 - scrape * 0.25 + curvature * 0.08, 0.4, 1.0);
  mat.F0 = mix(vec3(0.08), color, 0.85);
  return mat;
}

vec3 evaluateLight(const MaterialSample mat, const Light light, vec3 V) {
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
  vec3 F = fresnelSchlick(mat.F0, max(dot(H, V), 0.0));

  vec3 numerator = NDF * G * F;
  float denom = max(4.0 * NdotV * NdotL, 0.001);
  vec3 specular = numerator / denom;
  specular *= mix(vec3(1.0), mat.albedo, 0.18 * (1.0 - mat.metallic));

  vec3 kS = F;
  vec3 kD = (vec3(1.0) - kS) * (1.0 - mat.metallic);
  vec3 diffuse = kD * mat.albedo / PI;

  vec3 radiance = light.color * light.intensity;
  return (diffuse + specular) * radiance * NdotL;
}

// ----------------------------- Main -----------------------------
void main() {
  vec3 baseColor = u_color;
  if (u_useTexture) {
    baseColor *= texture(u_texture, v_texCoord).rgb;
  }

  float Y = luma(baseColor);
  float S = sat(baseColor);
  float blueRatio = baseColor.b / max(baseColor.r, 0.001);

  bool likelyLeather =
      (Y > 0.18 && Y < 0.65 && baseColor.r > baseColor.g * 1.03);
  bool likelyLinen = (Y > 0.65 && S < 0.22);
  bool likelyBronze = (baseColor.r > baseColor.g * 1.03 &&
                       baseColor.r > baseColor.b * 1.10 && Y > 0.42);
  float leatherDist =
      min(colorDistance(baseColor, REF_LEATHER),
          colorDistance(baseColor, REF_LEATHER_DARK));
  bool paletteLeather = leatherDist < 0.18;
  bool looksWood =
      (blueRatio > 0.42 && blueRatio < 0.8 && Y < 0.55 && S < 0.55) ||
      colorDistance(baseColor, REF_WOOD) < 0.12;
  bool looksCloth =
      colorDistance(baseColor, REF_CLOTH) < 0.22 ||
      (baseColor.b > baseColor.g * 1.25 && baseColor.b > baseColor.r * 1.35);
  bool looksSkin =
      colorDistance(baseColor, REF_SKIN) < 0.2 ||
      (S < 0.35 && baseColor.r > 0.55 && baseColor.g > 0.35 &&
       baseColor.b > 0.28);
  bool looksBeard =
      (!looksSkin &&
       (colorDistance(baseColor, REF_BEARD) < 0.16 || (Y < 0.32 && S < 0.4)));
  bool looksMetal =
      colorDistance(baseColor, REF_METAL) < 0.18 ||
      (S < 0.15 && Y > 0.4 && baseColor.b > baseColor.r * 0.9);

  bool preferLeather = (paletteLeather && blueRatio < 0.42) ||
                       (likelyLeather && !looksWood && blueRatio < 0.4);
  bool isHelmetRegion = v_bodyHeight > 0.92;
  bool isFaceRegion = v_bodyHeight > 0.45 && v_bodyHeight < 0.92;

  vec3 Nw = normalize(v_worldNormal);
  vec3 Tw = normalize(v_tangent);
  vec3 Bw = normalize(v_bitangent);

  vec3 V = normalize(vec3(0.0, 0.0, 1.0));

  float rain = clamp(u_rainIntensity, 0.0, 1.0);
  float curvature = computeCurvature(Nw);

  float streak = smoothstep(0.65, 1.0,
                            sin(v_worldPos.y * 22.0 - u_time * 4.0 +
                                fbm(v_worldPos * 0.8, Nw, 1.2) * 6.283));
  float wetGather = (1.0 - clamp(Nw.y, 0.0, 1.0)) *
                    (0.4 + 0.6 * fbm(v_worldPos * 2.0, Nw, 3.0));
  float wetMask =
      clamp(rain * mix(0.5 * wetGather, 1.0 * wetGather, streak), 0.0, 1.0);

  MaterialSample material =
      makeFallbackSample(baseColor, Nw, Tw, Bw, v_worldPos, wetMask, curvature);
  if (looksMetal && isHelmetRegion) {
    material = makeMetalSample(CANON_HELMET, Nw, Tw, Bw, v_worldPos, wetMask,
                               curvature);
  } else if (looksSkin && isFaceRegion) {
    material = makeSkinSample(CANON_SKIN, Nw, Tw, Bw, v_worldPos, wetMask,
                              curvature);
  } else if (looksBeard && isFaceRegion) {
    material = makeHairSample(CANON_BEARD, Nw, Tw, Bw, v_worldPos, wetMask);
  } else if (looksWood) {
    vec3 woodColor = mix(baseColor, CANON_WOOD, 0.35);
    material =
        makeWoodSample(woodColor, Nw, Tw, Bw, v_worldPos, wetMask, curvature);
  } else if (looksCloth) {
    material =
        makeClothSample(baseColor, Nw, Tw, Bw, v_worldPos, wetMask, curvature);
  } else if (preferLeather) {
    material = makeLeatherSample(baseColor, Nw, Tw, Bw, v_worldPos,
                                 clamp(v_leatherTension, 0.0, 1.0),
                                 clamp(v_bodyHeight, 0.0, 1.0), v_armorLayer,
                                 wetMask, curvature);
  } else if (likelyLinen) {
    material = makeLinenSample(baseColor, Nw, Tw, Bw, v_worldPos,
                               clamp(v_bodyHeight, 0.0, 1.0), wetMask,
                               curvature);
  } else if (likelyBronze) {
    material =
        makeBronzeSample(baseColor, Nw, Tw, Bw, v_worldPos, wetMask, curvature);
  }

  Light lights[2];
  lights[0].dir = normalize(vec3(0.55, 1.15, 0.35));
  lights[0].color = vec3(1.15, 1.05, 0.95);
  lights[0].intensity = 1.35;
  lights[1].dir = normalize(vec3(-0.35, 0.65, -0.45));
  lights[1].color = vec3(0.35, 0.45, 0.65);
  lights[1].intensity = 0.35;

  vec3 lightAccum = vec3(0.0);
  for (int i = 0; i < 2; ++i) {
    vec3 contribution = evaluateLight(material, lights[i], V);
    if (wetMask > 0.001) {
      contribution += clearcoatSpec(material.normal, lights[i].dir, V,
                                    wetMask * 0.8,
                                    mix(0.10, 0.03, wetMask)) *
                      lights[i].color * lights[i].intensity;
    }
    lightAccum += contribution;
  }

  vec3 ambient = computeAmbient(material.normal) * material.albedo *
                 material.ao * 0.35;
  vec3 bounce =
      vec3(0.45, 0.34, 0.25) * (0.15 + 0.45 * clamp(-material.normal.y, 0.0, 1.0));
  vec3 color = lightAccum + ambient + bounce * (1.0 - material.metallic) * 0.25;

  color = mix(color, color * 0.85, wetMask * 0.2);
  color = toneMapAndGamma(max(color, vec3(0.0)));

  FragColor = vec4(clamp(color, 0.0, 1.0), u_alpha);
}
