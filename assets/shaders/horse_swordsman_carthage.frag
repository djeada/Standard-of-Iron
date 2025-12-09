#version 330 core

in vec3 v_normal;
in vec2 v_texCoord;
in vec3 v_worldPos;
in float v_armorLayer;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_useTexture;
uniform float u_alpha;
uniform int u_materialId;

out vec4 FragColor;

const float PI = 3.14159265359;

const vec3 BRONZE_BASE_COLOR = vec3(0.86, 0.66, 0.36);

const vec3 DARK_METAL_COLOR = vec3(0.14, 0.14, 0.16);

const vec3 SHIELD_BROWN_COLOR = vec3(0.18, 0.09, 0.035);

float saturatef(float x) { return clamp(x, 0.0, 1.0); }
vec3 saturate3(vec3 x) { return clamp(x, 0.0, 1.0); }

vec3 boostSaturation(vec3 color, float amount) {
  float grey = dot(color, vec3(0.299, 0.587, 0.114));
  return mix(vec3(grey), color, 1.0 + amount);
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
  float v = 0.0;
  float a = 0.5;
  mat2 rot = mat2(0.87, 0.50, -0.50, 0.87);
  for (int i = 0; i < 4; ++i) {
    v += a * noise(p);
    p = rot * p * 2.0 + vec2(100.0);
    a *= 0.5;
  }
  return v;
}

float D_GGX(float NdotH, float roughness) {
  float a = roughness * roughness;
  float a2 = a * a;
  float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
  return a2 / (PI * denom * denom + 1e-6);
}

float G_SchlickGGX(float NdotX, float roughness) {
  float r = roughness + 1.0;
  float k = (r * r) / 8.0;
  return NdotX / (NdotX * (1.0 - k) + k + 1e-6);
}

float G_Smith(float NdotV, float NdotL, float roughness) {
  return G_SchlickGGX(NdotV, roughness) * G_SchlickGGX(NdotL, roughness);
}

vec3 F_Schlick(float cosTheta, vec3 F0) {
  float t = 1.0 - cosTheta;
  return F0 + (1.0 - F0) * (t * t * t * t * t);
}

float hammerPattern(vec3 pos) {
  float coarse = fbm(pos.xz * 14.0);
  float fine = fbm(pos.xy * 30.0 + 5.3);
  float micro = noise(pos.yz * 50.0);
  return coarse * 0.45 + fine * 0.35 + micro * 0.2;
}

float scaleArmor(vec2 uv) {
  vec2 id = floor(uv * 9.0);
  vec2 f = fract(uv * 9.0);
  float offset = mod(id.y, 2.0) * 0.5;
  f.x = fract(f.x + offset);
  float d = length((f - 0.5) * vec2(1.0, 1.4));
  float edge = smoothstep(0.52, 0.42, d);
  float highlight = smoothstep(0.32, 0.22, d);
  return edge + highlight * 0.5;
}

float chainmailRings(vec2 p) {
  vec2 uv = p * 28.0;
  vec2 g0 = fract(uv) - 0.5;
  float r0 = length(g0);
  float ring0 = smoothstep(0.32, 0.28, r0) - smoothstep(0.22, 0.18, r0);
  vec2 g1 = fract(uv + vec2(0.5, 0.0)) - 0.5;
  float r1 = length(g1);
  float ring1 = smoothstep(0.32, 0.28, r1) - smoothstep(0.22, 0.18, r1);
  return (ring0 + ring1) * 0.6;
}

float horseHide(vec2 p) {
  float grain = fbm(p * 60.0) * 0.08;
  float ripple = sin(p.x * 18.0) * cos(p.y * 22.0) * 0.03;
  float mottling = smoothstep(0.5, 0.7, fbm(p * 5.0)) * 0.06;
  return grain + ripple + mottling;
}

float leatherGrain(vec2 p) {
  float coarse = fbm(p * 12.0) * 0.15;
  float fine = noise(p * 35.0) * 0.08;
  return coarse + fine;
}

void main() {
  vec3 baseColor = clamp(u_color, 0.0, 1.0);
  if (u_useTexture)
    baseColor *= texture(u_texture, v_texCoord).rgb;
  baseColor = boostSaturation(baseColor, 0.25);
  vec3 vibrantTeamColor =
      clamp(boostSaturation(baseColor * vec3(1.08, 0.92, 0.94), 0.6), 0.0, 1.0);

  vec3 N = normalize(v_normal);
  vec3 V = normalize(vec3(0.0, 0.5, 1.0));
  vec3 L = normalize(vec3(0.5, 1.0, 0.4));
  vec3 H = normalize(L + V);

  bool is_rider_skin = (u_materialId == 0);
  bool is_body_armor = (u_materialId == 1);
  bool is_helmet = (u_materialId == 2);
  bool is_weapon = (u_materialId == 3);
  bool is_shield = (u_materialId == 4);
  bool is_rider_clothing = (u_materialId == 5);
  bool is_horse_hide = (u_materialId == 6);
  bool is_horse_mane = (u_materialId == 7);
  bool is_horse_hoof = (u_materialId == 8);
  bool is_saddle_leather = (u_materialId == 9);
  bool is_bridle = (u_materialId == 10);
  bool is_saddle_blanket = (u_materialId == 11);

  vec3 albedo = baseColor;
  float metallic = 0.0;
  float roughness = 0.5;

  if (is_rider_skin) {
    albedo = mix(baseColor, vec3(0.92, 0.76, 0.62), 0.25);
    albedo = boostSaturation(albedo, 0.15);
    float armMask = smoothstep(0.3, 0.55, v_worldPos.y) *
                    smoothstep(0.25, 0.45, abs(v_worldPos.x));
    float legMask = smoothstep(0.0, 0.35, v_worldPos.y) *
                    smoothstep(0.20, 0.48, abs(v_worldPos.x));
    vec3 limbTint = mix(albedo * 0.9, vibrantTeamColor, 0.6);
    float limbBlend = clamp(max(armMask, legMask), 0.0, 1.0);
    albedo = mix(albedo, limbTint, limbBlend * 0.9);
    metallic = 0.0;
    roughness = 0.55;
  }

  else if (is_body_armor) {

    vec2 metalUV = v_texCoord * 6.0;
    float hammer = hammerPattern(vec3(metalUV, v_worldPos.y));
    float scales = scaleArmor(metalUV);
    float chain = chainmailRings(v_worldPos.xz);

    vec3 metal = DARK_METAL_COLOR;
    float patinaNoise = fbm(metalUV * 2.5);
    vec3 patinaTint = DARK_METAL_COLOR * 1.12;
    metal = mix(metal, patinaTint, patinaNoise * 0.20);

    vec3 teamTint = mix(vec3(1.0), baseColor, 0.25);
    albedo = metal * teamTint;

    albedo *= 0.9 + hammer * 0.25;
    albedo = mix(albedo, albedo * 1.20, scales * 0.45);
    albedo += vec3(0.05) * chain;

    metallic = 1.0;
    roughness = clamp(0.22 + hammer * 0.08 - scales * 0.08, 0.10, 0.33);
  }

  else if (is_helmet) {

    vec2 metalUV = v_texCoord * 8.0;
    float hammer = hammerPattern(vec3(metalUV, v_worldPos.y));
    float scales = scaleArmor(metalUV * 1.1);

    vec3 metal = DARK_METAL_COLOR;
    float patinaNoise = fbm(metalUV * 3.0);
    vec3 patinaTint = DARK_METAL_COLOR * 1.10;
    metal = mix(metal, patinaTint, patinaNoise * 0.18);

    float crest = smoothstep(0.8, 0.9, v_worldPos.y);
    crest *= smoothstep(0.45, 0.30, abs(v_worldPos.x));
    vec3 crestColor = boostSaturation(baseColor, 0.6) * 1.3;

    albedo = mix(metal, crestColor, crest * 0.5);

    albedo *= 0.9 + hammer * 0.25;
    albedo = mix(albedo, albedo * 1.18, scales * 0.50);

    metallic = 1.0;
    roughness = clamp(0.18 + hammer * 0.08 - scales * 0.08, 0.08, 0.28);
  }

  else if (is_weapon) {
    float h = v_worldPos.y;
    float blade = smoothstep(0.25, 0.48, h);
    float guard = smoothstep(0.15, 0.25, h) * (1.0 - blade);
    float polish = fbm(v_worldPos.xy * 40.0);

    vec3 handle = boostSaturation(baseColor * 0.85, 0.3);
    handle += vec3(0.05) * polish;

    vec3 guardCol = mix(BRONZE_BASE_COLOR, baseColor * 1.1, 0.3);
    guardCol = boostSaturation(guardCol, 0.35);

    vec3 steel = vec3(0.88, 0.90, 0.95);
    steel += vec3(0.06) * polish;
    steel = mix(steel, baseColor * 0.35 + vec3(0.55), 0.12);

    albedo = mix(handle, guardCol, guard);
    albedo = mix(albedo, steel, blade);

    metallic = mix(0.1, 1.0, blade + guard * 0.9);
    roughness = mix(0.5, 0.04, blade);
  }

  else if (is_shield) {
    float dist = length(v_worldPos.xz);
    float boss = smoothstep(0.28, 0.0, dist);
    float rings = sin(dist * 18.0) * 0.5 + 0.5;
    rings = smoothstep(0.35, 0.65, rings) * (1.0 - boss);

    vec3 faceColor =
        boostSaturation(mix(SHIELD_BROWN_COLOR, baseColor, 0.12), 0.35);
    vec3 bronze = BRONZE_BASE_COLOR;
    vec3 metalMix = mix(faceColor, bronze, boss + rings * 0.6);

    albedo = mix(faceColor, metalMix, boss + rings * 0.5);

    metallic = mix(0.25, 1.0, boss);
    roughness = mix(0.5, 0.10, boss);
  }

  else if (is_rider_clothing || is_saddle_blanket) {
    float weave = sin(v_worldPos.x * 55.0) * sin(v_worldPos.z * 55.0) * 0.04;
    float texture_var = fbm(v_worldPos.xz * 8.0);
    float rimGlow = smoothstep(0.2, 0.65, abs(v_worldPos.z));
    float foldDepth = smoothstep(0.15, 0.5, abs(v_worldPos.y));

    vec3 clothBase = mix(vibrantTeamColor, baseColor * 0.82, 0.12);
    vec3 highlight = mix(clothBase * 0.9, clothBase * 1.28,
                         clamp(weave + texture_var * 0.3, 0.0, 1.0));
    vec3 shadow = mix(clothBase * vec3(0.8, 0.9, 0.95), clothBase, 0.35);
    vec3 clothLayer = mix(shadow, highlight, foldDepth * 1.1);

    albedo = clothLayer * (1.0 + weave * 0.38 + texture_var * 0.08);
    albedo = mix(albedo, highlight, rimGlow * 0.45);

    metallic = 0.0;
    roughness = 0.62 + texture_var * 0.06 + foldDepth * 0.03;
  }

  else if (is_horse_hide) {
    float hide = horseHide(v_worldPos.xz);
    float grain = fbm(v_worldPos.xz * 22.0);

    albedo = boostSaturation(baseColor, 0.2);
    albedo *= 1.0 + hide * 0.08 - grain * 0.05;

    metallic = 0.0;
    roughness = 0.65;

    float sheen = pow(1.0 - max(dot(N, V), 0.0), 4.0) * 0.15;
    albedo += vec3(sheen) * 0.5;
  }

  else if (is_horse_mane) {
    float strand =
        sin(v_worldPos.x * 120.0) * 0.15 + noise(v_worldPos.xy * 100.0) * 0.1;

    albedo = baseColor * 0.4;
    albedo = boostSaturation(albedo, 0.15);
    albedo *= 1.0 + strand * 0.08;

    metallic = 0.0;
    roughness = 0.5;
  }

  else if (is_horse_hoof) {
    float grain = fbm(v_worldPos.xy * 15.0);
    albedo = baseColor * 0.35;
    albedo *= 1.0 + grain * 0.1;
    metallic = 0.0;
    roughness = 0.45;
  }

  else if (is_saddle_leather || is_bridle) {
    float grain = leatherGrain(v_worldPos.xz);
    float wear = fbm(v_worldPos.xz * 4.0) * 0.08;

    albedo = boostSaturation(baseColor, 0.3);
    albedo *= 1.0 + grain * 0.15 - wear;

    metallic = 0.0;
    roughness = 0.5 - wear * 0.1;

    float sheen = pow(1.0 - max(dot(N, V), 0.0), 5.0) * 0.12;
    albedo += vec3(sheen) * baseColor;
  }

  else {
    albedo = boostSaturation(baseColor, 0.2);
    metallic = 0.0;
    roughness = 0.6;
  }

  float NdotL = max(dot(N, L), 0.0);
  float NdotV = max(dot(N, V), 0.001);
  float NdotH = max(dot(N, H), 0.0);
  float VdotH = max(dot(V, H), 0.0);

  vec3 F0 = mix(vec3(0.04), albedo, metallic);

  float D = D_GGX(NdotH, max(roughness, 0.01));
  float G = G_Smith(NdotV, NdotL, roughness);
  vec3 F = F_Schlick(VdotH, F0);

  vec3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 0.001);
  vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
  vec3 diffuse = kD * albedo / PI;

  vec3 color = (diffuse + specular * 2.2) * NdotL * 2.1;

  if (metallic > 0.5) {
    vec3 R = reflect(-V, N);

    float specPower = 240.0 / (roughness * roughness + 0.001);
    specPower = min(specPower, 2000.0);
    float mirrorSpec = pow(max(dot(R, L), 0.0), specPower);
    color += albedo * mirrorSpec * 1.8;

    float hotspot = pow(NdotH, 500.0);
    color += vec3(1.2) * hotspot * (1.0 - roughness * 1.8);

    float softSpec = pow(max(dot(R, L), 0.0), 28.0);
    color += albedo * softSpec * 0.6;

    vec3 skyCol = vec3(0.55, 0.65, 0.85);
    vec3 groundCol = vec3(0.38, 0.32, 0.25);
    float upFace = R.y * 0.5 + 0.5;
    vec3 envReflect = mix(groundCol, skyCol, upFace);
    color += envReflect * (1.0 - roughness) * 0.5;
  }

  vec3 ambient = albedo * 0.42;

  vec3 skyAmbient = vec3(0.45, 0.55, 0.70);
  vec3 groundAmbient = vec3(0.30, 0.25, 0.20);
  float hemi = N.y * 0.5 + 0.5;

  ambient *= mix(groundAmbient, skyAmbient, hemi) * 1.4;

  float rim = pow(1.0 - NdotV, 3.5);
  ambient += vec3(0.35, 0.40, 0.50) * rim * 0.32;

  if (metallic > 0.5) {
    ambient += albedo * 0.16 * (1.0 - roughness);
  }

  color += ambient;

  color = color / (color + vec3(0.55));
  color = pow(color, vec3(0.9));

  FragColor = vec4(saturate3(color), u_alpha);
}
