#version 330 core

in vec3 v_worldNormal;
in vec3 v_worldPos;
in vec3 v_instanceColor;
in float v_instanceAlpha;
in vec2 v_texCoord;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_useTexture;
uniform float u_alpha;
uniform bool u_instanced;
uniform int u_materialId;

out vec4 FragColor;

const float PI = 3.14159265359;

const vec3 BRONZE_BASE_COLOR = vec3(0.86, 0.66, 0.36);

const vec3 DARK_METAL_COLOR = vec3(0.14, 0.14, 0.16);

const vec3 SHIELD_BROWN_COLOR = vec3(0.18, 0.09, 0.035);

float saturate(float x) { return clamp(x, 0.0, 1.0); }
vec3 saturate3(vec3 x) { return clamp(x, 0.0, 1.0); }

vec3 boostSaturation(vec3 color, float amount) {
  float grey = dot(color, vec3(0.299, 0.587, 0.114));
  return mix(vec3(grey), color, 1.0 + amount);
}

float hash2(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float a = hash2(i);
  float b = hash2(i + vec2(1.0, 0.0));
  float c = hash2(i + vec2(0.0, 1.0));
  float d = hash2(i + vec2(1.0, 1.0));
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
  float t5 = t * t * t * t * t;
  return F0 + (1.0 - F0) * t5;
}

float hammerPattern(vec3 pos) {
  float coarse = fbm(pos.xz * 12.0);
  float fine = fbm(pos.xy * 28.0 + 7.3);
  float micro = noise(pos.yz * 45.0);
  return coarse * 0.5 + fine * 0.35 + micro * 0.15;
}

float scaleArmor(vec2 uv) {
  vec2 id = floor(uv * 8.0);
  vec2 f = fract(uv * 8.0);
  float offset = mod(id.y, 2.0) * 0.5;
  f.x = fract(f.x + offset);
  float d = length((f - 0.5) * vec2(1.0, 1.5));
  float edge = smoothstep(0.55, 0.45, d);
  float highlight = smoothstep(0.35, 0.25, d);
  return edge + highlight * 0.4;
}

void main() {
  vec3 base_color_in = u_instanced ? v_instanceColor : u_color;
  float alpha_in = u_instanced ? v_instanceAlpha : u_alpha;
  vec3 baseColor = clamp(base_color_in, 0.0, 1.0);
  if (u_useTexture) {
    baseColor *= texture(u_texture, v_texCoord).rgb;
  }
  baseColor = boostSaturation(baseColor, 0.3);
  vec3 vibrantTeamColor =
      boostSaturation(baseColor * vec3(1.05, 0.95, 0.95), 0.55);

  bool is_skin = (u_materialId == 0);
  bool is_armor = (u_materialId == 1);
  bool is_helmet = (u_materialId == 2);
  bool is_weapon = (u_materialId == 3);
  bool is_shield = (u_materialId == 4);

  vec3 N = normalize(v_worldNormal);
  vec3 V = normalize(vec3(0.0, 0.0, 1.0));
  vec3 L = normalize(vec3(0.5, 1.0, 0.4));
  vec3 H = normalize(L + V);

  vec3 albedo = baseColor;
  float metallic = 0.0;
  float roughness = 0.5;

  if (is_skin) {
    albedo = mix(baseColor, vec3(0.95, 0.78, 0.65), 0.2);
    albedo = boostSaturation(albedo, 0.15);
    metallic = 0.0;
    roughness = 0.55;

    float pants = 1.0 - smoothstep(0.35, 0.60, v_worldPos.y);
    float grain = fbm(v_worldPos.xz * 18.0 + v_worldPos.y * 4.0);
    vec3 leather = baseColor * 0.75;
    leather = boostSaturation(leather, 0.2);
    leather -= vec3(0.08) * grain;
    albedo = mix(albedo, leather, pants);
    roughness = mix(roughness, 0.5, pants);

    float clothMask =
        clamp(max(pants, 1.0 - smoothstep(0.6, 0.85, v_worldPos.y)), 0.0, 1.0);
    float clothNoise = fbm(v_worldPos.xz * 22.0);
    vec3 clothTone = mix(vibrantTeamColor * 0.88, vibrantTeamColor * 1.12,
                         clothNoise * 0.5 + 0.25);
    albedo = mix(albedo, clothTone, clothMask * 0.75);

    float armBand = smoothstep(0.3, 0.55, v_worldPos.y) *
                    smoothstep(0.25, 0.45, abs(v_worldPos.x)) *
                    smoothstep(0.85, 0.45, abs(v_worldPos.z));
    float armAccent = clamp(armBand, 0.0, 1.0);
    vec3 limbTeamColor = mix(baseColor * 0.82, vibrantTeamColor, 0.72);
    albedo = mix(albedo, limbTeamColor, armAccent * 0.9);

  } else if (is_armor) {

    vec2 metalUV = v_texCoord * 6.0;
    float hammer = hammerPattern(vec3(metalUV, v_worldPos.y));
    float scales = scaleArmor(metalUV);

    vec3 metal = DARK_METAL_COLOR;
    float patinaNoise = fbm(metalUV * 2.5);
    vec3 patinaTint = DARK_METAL_COLOR * 1.15;
    metal = mix(metal, patinaTint, patinaNoise * 0.2);

    vec3 teamTint = mix(vec3(1.0), baseColor, 0.25);
    albedo = metal * teamTint;

    albedo *= 0.9 + hammer * 0.25;
    albedo = mix(albedo, albedo * 1.2, scales * 0.5);

    metallic = 1.0;
    roughness = 0.22 + hammer * 0.08 - scales * 0.08;
    roughness = clamp(roughness, 0.10, 0.35);
  } else if (is_helmet) {

    vec2 metalUV = v_texCoord * 8.0;
    float hammer = hammerPattern(vec3(metalUV, v_worldPos.y));
    float scales = scaleArmor(metalUV * 1.2);

    vec3 metal = DARK_METAL_COLOR;
    float patinaNoise = fbm(metalUV * 3.0);
    vec3 patinaTint = DARK_METAL_COLOR * 1.12;
    metal = mix(metal, patinaTint, patinaNoise * 0.18);

    float crest = smoothstep(0.8, 0.9, v_worldPos.y);
    crest *= smoothstep(0.4, 0.3, abs(v_worldPos.x));

    vec3 crestColor = boostSaturation(baseColor, 0.5) * 1.3;

    albedo = mix(metal, crestColor, crest * 0.5);

    albedo *= 0.9 + hammer * 0.25;
    albedo = mix(albedo, albedo * 1.18, scales * 0.5);

    metallic = 1.0;
    roughness = 0.18 + hammer * 0.08 - scales * 0.08;
    roughness = clamp(roughness, 0.08, 0.30);
  } else if (is_weapon) {
    float h = v_worldPos.y;
    float blade = smoothstep(0.28, 0.50, h);
    float guard = smoothstep(0.18, 0.28, h) * (1.0 - blade);

    float polish = fbm(v_worldPos.xy * 35.0);

    vec3 handle = boostSaturation(baseColor * 0.9, 0.25);
    handle += vec3(0.06) * polish;

    vec3 guardCol = mix(BRONZE_BASE_COLOR, baseColor * 1.1, 0.3);
    guardCol = boostSaturation(guardCol, 0.3);

    vec3 steel = vec3(0.85, 0.87, 0.92);
    steel += vec3(0.08) * polish;
    steel = mix(steel, baseColor * 0.4 + vec3(0.55), 0.15);

    albedo = mix(handle, guardCol, guard);
    albedo = mix(albedo, steel, blade);

    metallic = mix(0.15, 1.0, blade + guard * 0.9);
    roughness = mix(0.45, 0.05, blade);
  } else if (is_shield) {
    float dist = length(v_worldPos.xz);
    float boss = smoothstep(0.25, 0.0, dist);
    float rings = sin(dist * 20.0) * 0.5 + 0.5;
    rings = smoothstep(0.3, 0.7, rings) * (1.0 - boss);

    vec3 shieldFace =
        boostSaturation(mix(SHIELD_BROWN_COLOR, baseColor, 0.12), 0.35);
    vec3 bronze = BRONZE_BASE_COLOR;
    vec3 shieldMetal = mix(shieldFace, bronze, boss + rings * 0.6);

    albedo = mix(shieldFace, shieldMetal, boss + rings * 0.5);

    metallic = mix(0.2, 1.0, boss + rings * 0.7);
    roughness = mix(0.45, 0.12, boss);
  } else {
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

  vec3 color = (diffuse + specular * 2.0) * NdotL * 2.0;

  if (metallic > 0.5) {
    vec3 R = reflect(-V, N);

    float specPower = 196.0 / (roughness * roughness + 0.001);
    specPower = min(specPower, 1536.0);
    float mirrorSpec = pow(max(dot(R, L), 0.0), specPower);
    color += albedo * mirrorSpec * 1.6;

    float hotspot = pow(NdotH, 400.0);
    color += vec3(1.1) * hotspot * (1.0 - roughness * 1.6);

    float softSpec = pow(max(dot(R, L), 0.0), 32.0);
    color += albedo * softSpec * 0.5;

    vec3 skyCol = vec3(0.6, 0.7, 0.9);
    vec3 groundCol = vec3(0.4, 0.35, 0.28);
    float upFace = R.y * 0.5 + 0.5;
    vec3 envReflect = mix(groundCol, skyCol, upFace);
    color += envReflect * (1.0 - roughness) * 0.4;
  }

  vec3 ambient = albedo * 0.42;

  float rim = pow(1.0 - NdotV, 3.5);
  ambient += vec3(0.4, 0.45, 0.55) * rim * 0.28;

  if (metallic > 0.5) {
    ambient += albedo * 0.12 * (1.0 - roughness);
  }

  color += ambient;

  color = color / (color + vec3(0.6));
  color = pow(color, vec3(0.92));

  FragColor = vec4(saturate3(color), alpha_in);
}
