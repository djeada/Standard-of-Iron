#version 330 core

in vec3 v_normal;
in vec3 v_worldNormal;
in vec3 v_tangent;
in vec3 v_bitangent;
in vec2 v_texCoord;
in vec3 v_worldPos;
in float v_armorLayer;
in float v_bodyHeight;
in float v_clothFolds;
in float v_fabricWear;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_useTexture;
uniform float u_alpha;
uniform int u_materialId;

out vec4 FragColor;

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
  float sum = 0.0;
  float amp = 0.5;
  float freq = 1.0;
  for (int i = 0; i < 4; ++i) {
    sum += amp * noise(p * freq);
    freq *= 2.12;
    amp *= 0.48;
  }
  return sum;
}

float triplanar_noise(vec3 pos, vec3 normal, float scale) {
  vec3 w = abs(normal);
  w = max(w, vec3(0.0001));
  w /= (w.x + w.y + w.z);
  float xy = noise(pos.xy * scale);
  float yz = noise(pos.yz * scale);
  float zx = noise(pos.zx * scale);
  return xy * w.z + yz * w.x + zx * w.y;
}

float cloth_weave(vec2 p) {
  float warp = sin(p.x * 68.0) * 0.55 + sin(p.x * 132.0) * 0.20;
  float weft = sin(p.y * 66.0) * 0.55 + sin(p.y * 124.0) * 0.20;
  float cross = sin(p.x * 12.0 + p.y * 14.0) * 0.08;
  return warp * weft * 0.06 + cross * 0.04;
}

float phoenician_linen(vec2 p) {
  float weave = cloth_weave(p);
  float slub = fbm(p * 8.5) * 0.08;
  float fine_thread = noise(p * 90.0) * 0.08;
  float sun_kiss = noise(p * 2.8) * 0.04;
  return weave + slub + fine_thread + sun_kiss;
}

vec3 perturb_cloth_normal(vec3 N, vec3 T, vec3 B, vec2 uv, float warpFreq,
                          float weftFreq, float slubStrength) {
  float warp = sin(uv.x * warpFreq) * 0.06;
  float weft = sin(uv.y * weftFreq) * 0.06;
  float slub = fbm(uv * 7.0) * slubStrength;
  return normalize(N + T * (warp + slub) + B * (weft + slub * 0.6));
}

vec3 perturb_leather_normal(vec3 N, vec3 T, vec3 B, vec2 uv) {
  float grain = fbm(uv * 8.0) * 0.18;
  float pores = noise(uv * 32.0) * 0.10;
  float scars = noise(uv * 14.0 + vec2(3.7, -2.1)) * 0.06;
  return normalize(N + T * (grain + scars * 0.4) + B * (pores + scars * 0.3));
}

vec3 perturb_bronze_normal(vec3 N, vec3 T, vec3 B, vec2 uv) {
  float hammer = fbm(uv * 14.0) * 0.15;
  float ripple = noise(uv * 48.0) * 0.05;
  return normalize(N + T * hammer + B * (hammer * 0.4 + ripple));
}

float D_GGX(float NdotH, float a) {
  float a2 = a * a;
  float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
  return a2 / max(3.14159 * d * d, 1e-5);
}

float G_Smith(float NdotV, float NdotL, float a) {
  float k = (a + 1.0);
  k = (k * k) / 8.0;
  float g_v = NdotV / (NdotV * (1.0 - k) + k);
  float g_l = NdotL / (NdotL * (1.0 - k) + k);
  return g_v * g_l;
}

vec3 fresnel_schlick(vec3 F0, float cos_theta) {
  return F0 + (vec3(1.0) - F0) * pow(1.0 - cos_theta, 5.0);
}

vec3 compute_ambient(vec3 normal) {
  float up = clamp(normal.y, 0.0, 1.0);
  float down = clamp(-normal.y, 0.0, 1.0);
  vec3 sky = vec3(0.85, 0.92, 1.0);
  vec3 ground = vec3(0.45, 0.38, 0.32);
  return sky * (0.40 + 0.50 * up) + ground * (0.20 + 0.40 * down);
}

vec3 apply_lighting(vec3 albedo, vec3 N, vec3 V, vec3 L, float roughness,
                    float metallic, float ao, float sheen, float wrap) {
  float NdotL = max(dot(N, L), 0.0);
  float NdotV = max(dot(N, V), 0.0);
  float wrapped = clamp(NdotL * (1.0 - wrap) + wrap, 0.0, 1.0);
  NdotL = wrapped;

  vec3 H = normalize(L + V);
  float NdotH = max(dot(N, H), 0.0);
  float VdotH = max(dot(V, H), 0.0);

  float a = max(roughness * roughness, 0.03);
  float D = D_GGX(NdotH, a);
  float G = G_Smith(NdotV, NdotL, a);

  vec3 F0 = mix(vec3(0.04), albedo, metallic);
  vec3 F = fresnel_schlick(F0, VdotH);

  vec3 spec = (D * G * F) / max(4.0 * NdotV * NdotL + 1e-5, 1e-5);
  vec3 kd = (vec3(1.0) - F) * (1.0 - metallic);
  vec3 diffuse = kd * albedo / 3.14159;

  vec3 ambient = compute_ambient(N) * albedo;
  vec3 light = (diffuse + spec * (1.0 + sheen)) * NdotL;

  float ao_strength = mix(0.45, 1.0, clamp(ao, 0.0, 1.0));
  vec3 result = ambient * (0.80 + 0.40 * ao_strength) +
                light * (0.80 * ao_strength + 0.20);
  return result;
}

void main() {
  vec3 N = normalize(v_worldNormal);
  vec3 T = normalize(v_tangent);
  vec3 B = normalize(v_bitangent);
  vec2 uv = v_worldPos.xz * 4.5;

  vec3 base_color = clamp(u_color, 0.0, 1.0);
  if (u_useTexture) {
    base_color *= texture(u_texture, v_texCoord).rgb;
  }

  vec3 teamDefault = vec3(0.0);
  vec3 teamColor = clamp(mix(teamDefault, u_color, 0.75), 0.0, 1.0);

  bool is_body = (u_materialId == 0);
  bool is_tunic = (u_materialId == 1);
  bool is_leather = (u_materialId == 2);
  bool is_tools = (u_materialId == 3);

  vec3 V = normalize(vec3(0.0, 1.4, 3.0) - v_worldPos);
  vec3 L = normalize(vec3(2.0, 3.0, 1.5));

  float curvature = length(dFdx(N)) + length(dFdy(N));
  float ao_folds =
      clamp(1.0 - (v_clothFolds * 0.55 + curvature * 0.80), 0.35, 1.0);
  float dust_mask = smoothstep(0.22, 0.0, v_bodyHeight);
  float sun_bleach = smoothstep(0.55, 1.05, v_bodyHeight) * 0.09;

  vec3 albedo = base_color;
  vec3 N_used = N;
  float roughness = 0.55;
  float metallic = 0.0;
  float sheen = 0.0;
  float wrap = 0.44;
  float ao = ao_folds;

  if (is_body) {
    vec3 skin_base = vec3(0.08, 0.07, 0.065);
    float legs = smoothstep(0.05, 0.50, v_bodyHeight) *
                 (1.0 - smoothstep(0.52, 0.70, v_bodyHeight));
    float limb_team = clamp(legs, 0.0, 1.0);
    skin_base = mix(skin_base, mix(skin_base, teamColor, 0.92), limb_team);
    float tone_noise = fbm(v_worldPos.xz * 3.1) - 0.5;
    albedo = clamp(skin_base + vec3(tone_noise) * 0.04, 0.0, 1.0);
    float skin_detail = noise(uv * 24.0) * 0.06;
    float subdermal = noise(uv * 7.0) * 0.05;
    N_used = normalize(N + vec3(0.0, 0.01, 0.0) *
                               triplanar_noise(v_worldPos * 3.0, N, 5.5));
    albedo *= 1.0 + skin_detail;
    albedo += vec3(0.03, 0.015, 0.0) * subdermal;
    albedo *= 1.18;
    float rim = pow(1.0 - clamp(dot(N_used, V), 0.0, 1.0), 4.0) * 0.08;
    albedo += vec3(rim);
    sheen = 0.08 + subdermal * 0.2;
    wrap = 0.46;
  } else if (is_tunic) {
    float linen = phoenician_linen(uv);
    float weave = cloth_weave(uv);
    float drape_folds = v_clothFolds * noise(uv * 9.0) * 0.18;
    float dust = dust_mask * (0.12 + noise(uv * 7.0) * 0.12);
    N_used = perturb_cloth_normal(N, T, B, uv, 128.0, 116.0, 0.08);
    vec3 tunic_base = vec3(0.68, 0.54, 0.38);
    albedo = tunic_base;
    albedo *= 1.02 + linen + weave * 0.08 - drape_folds * 0.5;
    albedo += vec3(0.04, 0.03, 0.0) * sun_bleach;
    albedo -= vec3(dust * 0.20);
    roughness = 0.66 - clamp(v_fabricWear * 0.08, 0.0, 0.12);
    sheen = 0.10 + clamp(v_bodyHeight * 0.04, 0.0, 0.06);
    ao *= 1.0 - dust * 0.30;
    wrap = 0.54;
  } else if (is_leather) {
    float leather_grain = fbm(uv * 8.0) * 0.16;
    float craft_detail = noise(uv * 28.0) * 0.07;
    float stitching = step(0.92, fract(v_worldPos.x * 14.0)) *
                      step(0.92, fract(v_worldPos.y * 12.0)) * 0.08;
    float edge_wear =
        smoothstep(0.86, 0.94, abs(dot(N, normalize(T + B)))) * 0.08;
    N_used = perturb_leather_normal(N, T, B, uv);
    vec3 leather_base = vec3(0.44, 0.30, 0.18);
    albedo = leather_base;
    float belt_band = smoothstep(0.47, 0.49, v_bodyHeight) -
                      smoothstep(0.53, 0.55, v_bodyHeight);
    albedo *= 1.06 + leather_grain + craft_detail - 0.04;
    albedo += vec3(stitching + edge_wear);
    albedo = mix(albedo, mix(albedo, teamColor, 0.80), belt_band);
    roughness = 0.52 - clamp(v_fabricWear * 0.05, 0.0, 0.10);
    sheen = 0.11;
    wrap = 0.46;
  } else if (is_tools) {
    float patina = noise(uv * 14.0) * 0.15 + fbm(uv * 22.0) * 0.10;
    float edge_polish =
        smoothstep(0.86, 0.95, abs(dot(N, normalize(T + B)))) * 0.14;
    N_used = perturb_bronze_normal(N, T, B, uv);
    vec3 bronze_default = vec3(0.78, 0.58, 0.32);
    float custom_weight =
        clamp(max(max(base_color.r, base_color.g), base_color.b), 0.0, 1.0);
    vec3 bronze_base = mix(bronze_default, base_color, custom_weight);
    bronze_base = max(bronze_base, vec3(0.02));
    albedo = bronze_base;
    albedo -= vec3(patina * 0.18);
    albedo += vec3(edge_polish);
    roughness = 0.30 + patina * 0.10;
    metallic = 0.92;
    sheen = 0.16;
    wrap = 0.42;
  } else {
    float detail = noise(uv * 12.0) * 0.10;
    albedo = mix(vec3(0.6, 0.6, 0.6), teamColor, 0.25);
    if (u_useTexture) {
      albedo *= max(texture(u_texture, v_texCoord).rgb, vec3(0.35));
    }
    albedo *= 1.0 + detail - 0.03;
  }

  vec3 color = apply_lighting(albedo, N_used, V, L, roughness, metallic, ao,
                              sheen, wrap);
  color = pow(color * 1.25, vec3(0.9));
  FragColor = vec4(clamp(color, 0.0, 1.0), u_alpha);
}
