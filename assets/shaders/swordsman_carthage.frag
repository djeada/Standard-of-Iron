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

const vec3 kBronzeBase = vec3(0.60, 0.42, 0.18);
const vec3 kLinenBase = vec3(0.88, 0.82, 0.72);
const vec3 kLeatherBase = vec3(0.38, 0.25, 0.15);

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

vec3 hemiAmbient(vec3 n) {
  float up = clamp(n.y * 0.5 + 0.5, 0.0, 1.0);
  vec3 sky = vec3(0.57, 0.66, 0.78);
  vec3 ground = vec3(0.32, 0.26, 0.20);
  return mix(ground, sky, up);
}

float D_GGX(float NdotH, float a) {
  float a2 = a * a;
  float d = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
  return a2 / max(3.14159265 * d * d, 1e-6);
}

float geometrySchlickGGX(float NdotX, float k) {
  return NdotX / max(NdotX * (1.0 - k) + k, 1e-6);
}

float geometrySmith(float NdotV, float NdotL, float roughness) {
  float r = roughness + 1.0;
  float k = (r * r) / 8.0;
  return geometrySchlickGGX(NdotV, k) * geometrySchlickGGX(NdotL, k);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
  return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
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

MaterialSample sampleHammeredBronze(vec3 baseColor, vec3 pos, vec3 N, vec3 T,
                                    vec3 B) {
  MaterialSample m;
  float hammer = fbm(pos * 12.0);
  float patina = fbm(pos * 5.0 + vec3(2.7, 0.3, 5.5));
  vec3 Np =
      perturb(N, T, B, vec3((hammer - 0.5) * 0.12, (patina - 0.5) * 0.07, 0.0));

  vec3 tint = mix(kBronzeBase, baseColor, 0.35);
  tint = mix(tint, vec3(0.20, 0.47, 0.40), clamp(patina * 0.55, 0.0, 0.6));
  tint += vec3(0.05) * pow(max(dot(Np, vec3(0.0, 1.0, 0.1)), 0.0), 5.0);

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.3 + hammer * 0.25 + patina * 0.18, 0.18, 0.72);
  m.F0 = mix(vec3(0.06), vec3(0.92, 0.66, 0.46), 0.9);
  return m;
}

MaterialSample sampleMuscleBronze(vec3 baseColor, vec3 pos, vec3 N, vec3 T,
                                  vec3 B, float sculpt, float frontMask) {
  MaterialSample m;
  float hammer = fbm(pos * 9.0);
  float profile = sculpt * 2.0 - 1.0;
  float chestLine = sin(pos.x * 6.0 + profile * 3.5);
  float abDivide = sin(pos.y * 11.0 - profile * 4.0);
  vec3 Np = perturb(N, T, B,
                    vec3((chestLine + profile * 0.6) * 0.06,
                         (abDivide + frontMask * 0.4) * 0.05, 0.0));

  vec3 tint = mix(kBronzeBase, baseColor, 0.65);
  tint = mix(tint, tint * vec3(1.05, 0.98, 0.90),
             smoothstep(-0.2, 0.8, profile) * 0.35);
  tint += vec3(0.08) * frontMask * smoothstep(0.45, 0.95, sculpt);
  tint -= vec3(0.04) * hammer;

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.28 + hammer * 0.25 - frontMask * 0.05, 0.16, 0.70);
  m.F0 = mix(vec3(0.08), vec3(0.94, 0.68, 0.44), 0.85);
  return m;
}

MaterialSample sampleChainmail(vec3 baseColor, vec3 pos, vec3 N, vec3 T, vec3 B,
                               float bandMix) {
  MaterialSample m;
  vec2 uv = pos.xz * 12.0 + pos.yx * 6.0;
  float ringA = sin(uv.x) * cos(uv.y);
  float ringB = sin((uv.x + uv.y) * 0.5);
  float ringPattern = mix(ringA, ringB, 0.5);
  float weave = fbm(vec3(uv, 0.0) * 0.6 + v_layerNoise);
  vec3 Np = perturb(
      N, T, B, vec3((ringPattern - 0.5) * 0.05, (bandMix - 0.5) * 0.04, 0.0));

  vec3 tint = mix(vec3(0.46, 0.48, 0.53), baseColor, 0.3);
  tint *= 1.0 - weave * 0.12;
  tint += vec3(0.05) * smoothstep(0.4, 0.9, bandMix);

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.42 - ringPattern * 0.08 + weave * 0.18, 0.2, 0.85);
  m.F0 = vec3(0.14);
  return m;
}

MaterialSample sampleLamellarLinen(vec3 baseColor, vec3 pos, vec3 N, vec3 T,
                                   vec3 B) {
  MaterialSample m;
  float slat = sin(pos.x * 32.0 + v_plateStress * 1.5);
  float seam = v_lamellaPhase;
  float edge = smoothstep(0.92, 1.0, seam) + smoothstep(0.0, 0.08, seam);
  float weave = fbm(pos * 6.0 + vec3(v_layerNoise));
  vec3 Np = perturb(N, T, B, vec3(slat * 0.04, weave * 0.03, 0.0));

  vec3 tint = mix(kLinenBase, baseColor, 0.4);
  tint *= 1.0 - 0.12 * weave;
  tint = mix(tint, tint * vec3(0.82, 0.76, 0.70),
             smoothstep(0.55, 1.0, v_bodyHeight));

  float plateHighlight = edge * 0.10;
  float rivetNoise = smoothstep(0.85, 1.0, seam) *
                     hash21(vec2(pos.x * 9.0, pos.y * 7.0)) * 0.04;

  m.color = tint + vec3(plateHighlight + rivetNoise);
  m.normal = Np;
  m.roughness = clamp(0.62 + weave * 0.18 - edge * 0.1, 0.35, 0.9);
  m.F0 = vec3(0.028);
  return m;
}

MaterialSample sampleDyedLeather(vec3 baseColor, vec3 pos, vec3 N, vec3 T,
                                 vec3 B) {
  MaterialSample m;
  float grain = fbm(pos * 4.0);
  float crack = fbm(pos * 9.0 + vec3(0.0, 1.7, 2.3));
  vec3 Np =
      perturb(N, T, B, vec3((grain - 0.5) * 0.08, (crack - 0.5) * 0.06, 0.0));

  vec3 tint = mix(kLeatherBase, baseColor, 0.5);
  tint *= 1.0 - 0.12 * grain;
  tint += vec3(0.05) * smoothstep(0.4, 0.9, crack);

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.55 + grain * 0.25 - crack * 0.18, 0.25, 0.95);
  m.F0 = vec3(0.035);
  return m;
}

void main() {
  vec3 baseColor = u_color;
  if (u_useTexture) {
    baseColor *= texture(u_texture, v_texCoord).rgb;
  }

  // Material ID: 0=body/skin, 1=armor, 2=helmet, 3=weapon, 4=shield
  bool isArmor = (u_materialId == 1);
  bool isHelmet = (u_materialId == 2);
  bool isWeapon = (u_materialId == 3);
  bool isShield = (u_materialId == 4);

  // Fallback to old layer system for non-specific meshes
  if (u_materialId == 0) {
    isHelmet = (v_armorLayer < 0.5);
    isArmor = (v_armorLayer >= 0.5 && v_armorLayer < 1.5);
  }

  bool helmetRegion = isHelmet;
  bool torsoRegion = isArmor || (u_materialId == 0 && v_armorLayer >= 0.5 &&
                                 v_armorLayer < 1.5);

  vec3 Nw = normalize(v_worldNormal);
  vec3 Tw = normalize(v_tangent);
  vec3 Bw = normalize(v_bitangent);

  MaterialSample mat;
  if (helmetRegion) {
    mat = sampleHammeredBronze(baseColor, v_worldPos, Nw, Tw, Bw);
  } else if (torsoRegion) {
    MaterialSample bronze = sampleMuscleBronze(
        baseColor, v_worldPos, Nw, Tw, Bw, v_cuirassProfile, v_frontMask);
    MaterialSample mail = sampleChainmail(
        baseColor, v_worldPos * 1.1, Nw, Tw, Bw,
        clamp(v_chainmailMix + (1.0 - v_frontMask) * 0.25, 0.0, 1.0));
    float mailBlend = smoothstep(0.3, 0.85, v_chainmailMix) *
                      smoothstep(0.1, 0.85, 1.0 - v_frontMask);
    float bronzeBlend = 1.0 - mailBlend;
    mat.color = bronze.color * bronzeBlend + mail.color * mailBlend;
    mat.normal =
        normalize(bronze.normal * bronzeBlend + mail.normal * mailBlend);
    mat.roughness = mix(bronze.roughness, mail.roughness, mailBlend);
    mat.F0 = mix(bronze.F0, mail.F0, mailBlend);
  } else {
    MaterialSample greave =
        sampleHammeredBronze(baseColor, v_worldPos * 1.2, Nw, Tw, Bw);
    MaterialSample skirtMail =
        sampleChainmail(baseColor, v_worldPos * 0.9, Nw, Tw, Bw,
                        clamp(v_chainmailMix + 0.35, 0.0, 1.0));
    MaterialSample skirtLeather =
        sampleDyedLeather(baseColor, v_worldPos * 0.8, Nw, Tw, Bw);
    float mailBias = smoothstep(0.25, 0.75, v_chainmailMix);
    float bronzeMix = smoothstep(0.2, 0.6, v_layerNoise);
    float skirtBlend = mix(mailBias, bronzeMix, 0.4);
    float mailWeight = clamp(skirtBlend, 0.0, 1.0);
    mat.color = mix(skirtLeather.color, greave.color, bronzeMix);
    mat.color = mix(mat.color, skirtMail.color, mailWeight * 0.7);
    mat.normal = normalize(mix(skirtLeather.normal, greave.normal, bronzeMix));
    mat.normal = normalize(mix(mat.normal, skirtMail.normal, mailWeight * 0.7));
    mat.roughness = mix(skirtLeather.roughness, greave.roughness, bronzeMix);
    mat.roughness = mix(mat.roughness, skirtMail.roughness, mailWeight * 0.7);
    mat.F0 = mix(skirtLeather.F0, greave.F0, bronzeMix);
    mat.F0 = mix(mat.F0, skirtMail.F0, mailWeight * 0.7);
  }

  vec3 L = normalize(vec3(0.45, 1.12, 0.35));
  vec3 V = normalize(vec3(0.0, 0.0, 1.0));
  vec3 H = normalize(L + V);

  float NdotL = max(dot(mat.normal, L), 0.0);
  float NdotV = max(dot(mat.normal, V), 0.0);
  float NdotH = max(dot(mat.normal, H), 0.0);
  float VdotH = max(dot(V, H), 0.0);

  float wrap = helmetRegion ? 0.15 : (torsoRegion ? 0.28 : 0.32);
  float diff = max(NdotL * (1.0 - wrap) + wrap, 0.18);

  float a = max(0.01, mat.roughness * mat.roughness);
  float D = D_GGX(NdotH, a);
  float G = geometrySmith(NdotV, NdotL, mat.roughness);
  vec3 F = fresnelSchlick(VdotH, mat.F0);
  vec3 spec = (D * G * F) / max(4.0 * NdotL * NdotV + 1e-5, 1e-5);

  float kd = 1.0 - max(max(F.r, F.g), F.b);
  if (helmetRegion) {
    kd *= 0.25;
  }

  vec3 ambient = hemiAmbient(mat.normal);
  vec3 color = mat.color;

  float grime = fbm(v_worldPos * vec3(2.0, 1.4, 2.3));
  color =
      mix(color, color * vec3(0.78, 0.72, 0.68), smoothstep(0.45, 0.9, grime));

  vec3 lighting =
      ambient * color * 0.55 + color * kd * diff + spec * max(NdotL, 0.0);

  FragColor = vec4(clamp(lighting, 0.0, 1.0), u_alpha);
}
