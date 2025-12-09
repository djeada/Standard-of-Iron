#version 330 core

in vec3 v_worldNormal;
in vec3 v_worldPos;
in vec2 v_texCoord;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_useTexture;
uniform float u_alpha;
uniform int u_materialId;

out vec4 FragColor;

// Simple PBR-ish helpers
float saturate(float v) { return clamp(v, 0.0, 1.0); }
vec3 saturate(vec3 v) { return clamp(v, 0.0, 1.0); }

float hash12(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

float fbm(vec2 p) {
  float v = 0.0;
  float a = 0.5;
  for (int i = 0; i < 4; ++i) {
    v += a * hash12(p);
    p *= 2.0;
    a *= 0.55;
  }
  return v;
}

float D_GGX(float NdotH, float a) {
  float a2 = a * a;
  float d = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
  return a2 / max(3.14159 * d * d, 1e-5);
}

float geometry_schlick(float NdotX, float k) {
  return NdotX / max(NdotX * (1.0 - k) + k, 1e-5);
}

float geometry_smith(float NdotV, float NdotL, float roughness) {
  float r = roughness + 1.0;
  float k = (r * r) / 8.0;
  return geometry_schlick(NdotV, k) * geometry_schlick(NdotL, k);
}

vec3 fresnel_schlick(float cos_theta, vec3 F0) {
  return F0 + (1.0 - F0) * pow(1.0 - cos_theta, 5.0);
}

void main() {
  vec3 base = u_color;
  if (u_useTexture) {
    base *= texture(u_texture, v_texCoord).rgb;
  }

  // Material layout: 0=skin, 1=armor, 2=helmet, 3=weapon, 4=shield
  bool is_skin = (u_materialId == 0);
  bool is_armor = (u_materialId == 1);
  bool is_helmet = (u_materialId == 2);
  bool is_weapon = (u_materialId == 3);

  vec3 N = normalize(v_worldNormal);
  vec3 V = normalize(vec3(0.0, 0.0, 1.0));
  vec3 L = normalize(vec3(0.5, 1.0, 0.4));
  vec3 H = normalize(L + V);

  // Lightweight leather armor for spearmen.
  float metallic = 0.0;
  float roughness = 0.55;
  vec3 albedo = base;

  if (is_skin) {
    albedo = mix(base, vec3(0.84, 0.72, 0.62), 0.35);
    metallic = 0.0;
    roughness = 0.6;
    // Jagged leather pants for lower body
    float pants_mask = 1.0 - smoothstep(0.38, 0.60, v_worldPos.y);
    float jag = fbm(v_worldPos.xz * 15.0 + v_worldPos.y * 3.0);
    vec3 leather = vec3(0.42, 0.30, 0.20) - vec3(0.04) * jag;
    albedo = mix(albedo, leather, pants_mask);
    roughness = mix(roughness, 0.55, pants_mask);
  } else if (is_armor || is_helmet) {
    vec3 leather_tint = vec3(0.46, 0.32, 0.20);
    // Add grain and scars to break up flatness.
    float grain = fbm(v_worldPos.xy * 14.0);
    float scar = fbm(v_worldPos.zy * 8.0 + vec2(1.7, 2.9));
    float wear = grain * 0.45 + scar * 0.30;
    albedo = mix(leather_tint, base, 0.4);
    albedo -= vec3(0.06) * wear;
    metallic = 0.05;
    roughness = mix(0.46, 0.65, wear);
  } else if (is_weapon) {
    // Spear: metal head + wooden shaft based on height.
    float tip = smoothstep(0.45, 0.65, v_worldPos.y);
    float shaft = 1.0 - tip;
    vec3 wood = vec3(0.52, 0.36, 0.22);
    vec3 steel = vec3(0.74, 0.76, 0.80);

    float wood_grain = fbm(v_worldPos.xz * 18.0 + v_worldPos.y * 4.0);
    float steel_brush = fbm(v_worldPos.xy * 32.0);

    vec3 wood_color = mix(wood, base, 0.35);
    wood_color += vec3(0.05) * wood_grain;

    vec3 steel_color = mix(steel, base, 0.25);
    steel_color += vec3(0.06) * steel_brush;

    albedo = mix(wood_color, steel_color, tip);
    metallic = mix(0.05, 0.85, tip);
    roughness = mix(0.50, 0.24, tip);
  }

  float NdotL = max(dot(N, L), 0.0);
  float NdotV = max(dot(N, V), 0.0);
  float NdotH = max(dot(N, H), 0.0);
  float VdotH = max(dot(V, H), 0.0);

  float a = max(0.02, roughness * roughness);
  float D = D_GGX(NdotH, a);
  float G = geometry_smith(NdotV, NdotL, roughness);
  vec3 F0 = mix(vec3(0.04), albedo, metallic);
  vec3 F = fresnel_schlick(VdotH, F0);
  vec3 spec = (D * G * F) / max(4.0 * NdotL * NdotV + 1e-5, 1e-5);

  vec3 kd = (vec3(1.0) - F) * (1.0 - metallic);
  vec3 diffuse = kd * albedo / 3.14159;

  float rim = pow(1.0 - max(dot(N, V), 0.0), 3.0);
  vec3 ambient = albedo * 0.34 + vec3(0.04) * rim;
  vec3 color = ambient + (diffuse + spec) * NdotL;

  FragColor = vec4(saturate(color), u_alpha);
}
