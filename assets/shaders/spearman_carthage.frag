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

const vec3 kLeatherBase = vec3(0.42, 0.30, 0.20);
const vec3 kLinenBase = vec3(0.88, 0.83, 0.74);
const vec3 kBronzeBase = vec3(0.58, 0.44, 0.20);

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

vec3 hemiAmbient(vec3 n) {
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

MaterialSample sampleLeather(vec3 baseColor, vec3 pos, vec3 N, vec3 T, vec3 B) {
  MaterialSample m;
  float grain = fbm(pos * 3.1 + vec3(v_layerNoise));
  float crease = fbm(pos * vec3(1.2, 4.0, 1.8));
  vec2 swell = vec2(grain - 0.5, crease - 0.5) * 0.15;
  vec3 Np = perturb(N, T, B, vec3(swell, v_bendAmount * 0.1));

  vec3 tint = mix(kLeatherBase, baseColor, 0.4);
  tint *= 1.0 - 0.18 * grain;
  tint += vec3(0.06) * smoothstep(0.4, 0.9, v_bodyHeight);
  tint = mix(tint, tint * vec3(0.7, 0.6, 0.5), smoothstep(0.65, 0.95, crease));

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.55 + grain * 0.3 - v_leatherTension * 0.2, 0.32, 0.95);
  m.F0 = vec3(0.035);
  return m;
}

MaterialSample sampleLinen(vec3 baseColor, vec3 pos, vec3 N, vec3 T, vec3 B) {
  MaterialSample m;
  float weaveU = sin(pos.x * 68.0);
  float weaveV = sin(pos.z * 72.0);
  float weft = weaveU * weaveV;
  float fray = fbm(pos * 5.0 + vec3(v_layerNoise));
  vec3 Np = perturb(N, T, B, vec3(weft * 0.05, fray * 0.04, 0.0));

  vec3 tint = mix(kLinenBase, baseColor, 0.5);
  tint *= 1.0 - 0.12 * fray;
  tint = mix(tint, tint * vec3(0.92, 0.9, 0.85),
             smoothstep(0.5, 1.0, v_bodyHeight));

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.78 + fray * 0.25, 0.35, 0.95);
  m.F0 = vec3(0.028);
  return m;
}

MaterialSample sampleBronze(vec3 baseColor, vec3 pos, vec3 N, vec3 T, vec3 B) {
  MaterialSample m;
  float hammer = fbm(pos * 12.5);
  float patina = fbm(pos * 6.0 + vec3(3.1, 0.2, 5.5));
  vec3 Np =
      perturb(N, T, B, vec3((hammer - 0.5) * 0.12, (patina - 0.5) * 0.08, 0.0));

  vec3 tint = mix(kBronzeBase, baseColor, 0.35);
  vec3 patinaColor = vec3(0.22, 0.5, 0.42);
  tint = mix(tint, patinaColor, clamp(patina * 0.55, 0.0, 0.6));
  tint += vec3(0.08) * pow(max(dot(Np, vec3(0.0, 1.0, 0.1)), 0.0), 6.0);

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.32 + hammer * 0.25 + patina * 0.2, 0.18, 0.72);
  m.F0 = mix(vec3(0.06), vec3(0.95, 0.68, 0.48), 0.85);
  return m;
}

vec3 applyWetDarkening(vec3 color, float wetMask) {
  return mix(color, color * 0.6, wetMask);
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
  bool upperRegion = isArmor || (u_materialId == 0 && v_armorLayer >= 0.5 && v_armorLayer < 1.5);

  vec3 Nw = normalize(v_worldNormal);
  vec3 Tw = normalize(v_tangent);
  vec3 Bw = normalize(v_bitangent);

  MaterialSample mat;
  if (helmetRegion) {
    mat = sampleBronze(baseColor, v_worldPos, Nw, Tw, Bw);
  } else if (upperRegion) {
    // Torso mixes linen and leather patches
    MaterialSample linen = sampleLinen(baseColor, v_worldPos, Nw, Tw, Bw);
    MaterialSample leather =
        sampleLeather(baseColor, v_worldPos * 1.3, Nw, Tw, Bw);
    float blend = smoothstep(0.3, 0.9, v_layerNoise);
    mat.color = mix(linen.color, leather.color, blend * 0.6);
    mat.normal = normalize(mix(linen.normal, leather.normal, blend * 0.5));
    mat.roughness = mix(linen.roughness, leather.roughness, blend);
    mat.F0 = mix(linen.F0, leather.F0, blend);
  } else {
    mat = sampleLeather(baseColor, v_worldPos * 1.1, Nw, Tw, Bw);
  }

  vec3 L = normalize(vec3(0.45, 1.15, 0.35));
  vec3 V = normalize(vec3(0.0, 0.0, 1.0));
  vec3 H = normalize(L + V);

  float NdotL = max(dot(mat.normal, L), 0.0);
  float NdotV = max(dot(mat.normal, V), 0.0);
  float NdotH = max(dot(mat.normal, H), 0.0);
  float VdotH = max(dot(V, H), 0.0);

  float wrap = helmetRegion ? 0.18 : 0.34;
  float diff = max(NdotL * (1.0 - wrap) + wrap, 0.0);

  float a = max(0.01, mat.roughness * mat.roughness);
  float D = D_GGX(NdotH, a);
  float G = geometrySmith(NdotV, NdotL, mat.roughness);
  vec3 F = fresnelSchlick(VdotH, mat.F0);
  vec3 spec = (D * G * F) / max(4.0 * NdotL * NdotV + 1e-5, 1e-5);

  float kd = 1.0 - max(max(F.r, F.g), F.b);
  if (helmetRegion) {
    kd *= 0.2;
  }

  float rain = clamp(u_rainIntensity, 0.0, 1.0);
  float wetMask = rain * (1.0 - clamp(mat.normal.y, 0.0, 1.0)) *
                  (0.4 + 0.6 * fbm(v_worldPos * vec3(1.4, 0.8, 1.2)));

  vec3 ambient = hemiAmbient(mat.normal);
  vec3 color = applyWetDarkening(mat.color, wetMask);

  vec3 lighting =
      ambient * color * 0.55 + color * kd * diff + spec * max(NdotL, 0.0);

  float grime = fbm(v_worldPos * 2.6 + vec3(v_layerNoise, v_bendAmount, 0.0));
  lighting = mix(lighting, lighting * vec3(0.78, 0.74, 0.70),
                 smoothstep(0.5, 0.95, grime));

  FragColor = vec4(clamp(lighting, 0.0, 1.0), u_alpha);
}
