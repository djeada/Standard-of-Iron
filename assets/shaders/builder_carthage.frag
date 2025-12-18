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
  float slub = fbm(p * 9.0) * 0.09;
  float fine_thread = noise(p * 94.0) * 0.09;
  float sun_kiss = noise(p * 3.0) * 0.05;
  float natural_texture = noise(p * 6.2) * 0.04;
  return weave + slub + fine_thread + sun_kiss + natural_texture;
}

vec3 perturb_cloth_normal(vec3 N, vec3 T, vec3 B, vec2 uv, float warpFreq,
                          float weftFreq, float slubStrength) {
  float warp = sin(uv.x * warpFreq) * 0.07;
  float weft = sin(uv.y * weftFreq) * 0.07;
  float slub = fbm(uv * 7.5) * slubStrength;
  float micro_detail = noise(uv * 115.0) * 0.02;
  return normalize(N + T * (warp + slub + micro_detail) + B * (weft + slub * 0.6 + micro_detail * 0.4));
}

vec3 perturb_leather_normal(vec3 N, vec3 T, vec3 B, vec2 uv) {
  float grain = fbm(uv * 8.5) * 0.20;
  float pores = noise(uv * 34.0) * 0.11;
  float scars = noise(uv * 15.0 + vec2(3.9, -2.3)) * 0.07;
  float crease = fbm(uv * 4.8) * 0.06;
  return normalize(N + T * (grain + scars * 0.4 + crease) + B * (pores + scars * 0.3 + crease * 0.5));
}

vec3 perturb_bronze_normal(vec3 N, vec3 T, vec3 B, vec2 uv) {
  float hammer = fbm(uv * 15.0) * 0.17;
  float ripple = noise(uv * 52.0) * 0.06;
  float polish = smoothstep(0.3, 0.7, noise(uv * 8.5)) * 0.04;
  return normalize(N + T * (hammer + polish) + B * (hammer * 0.4 + ripple + polish * 0.3));
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
    float tone_noise = fbm(v_worldPos.xz * 3.2) - 0.5;
    albedo = clamp(skin_base + vec3(tone_noise) * 0.05, 0.0, 1.0);
    float skin_detail = noise(uv * 26.0) * 0.07;
    float subdermal = noise(uv * 7.5) * 0.06;
    float micro_detail = noise(uv * 50.0) * 0.03;
    N_used = normalize(N + vec3(0.0, 0.01, 0.0) *
                               triplanar_noise(v_worldPos * 3.2, N, 6.0));
    albedo *= 1.0 + skin_detail + micro_detail;
    albedo += vec3(0.035, 0.018, 0.0) * subdermal;
    albedo *= 1.20;
    float rim = pow(1.0 - clamp(dot(N_used, V), 0.0, 1.0), 3.8) * 0.09;
    albedo += vec3(rim);
    sheen = 0.10 + subdermal * 0.25;
    wrap = 0.48;
  } else if (is_tunic) {
    float linen = phoenician_linen(uv);
    float weave = cloth_weave(uv);
    float drape_folds = v_clothFolds * noise(uv * 9.5) * 0.20;
    float dust = dust_mask * (0.13 + noise(uv * 7.5) * 0.13);
    N_used = perturb_cloth_normal(N, T, B, uv, 132.0, 120.0, 0.09);
    vec3 tunic_base = vec3(0.70, 0.56, 0.40);
    albedo = tunic_base;
    albedo *= 1.03 + linen + weave * 0.09 - drape_folds * 0.5;
    albedo += vec3(0.045, 0.035, 0.0) * sun_bleach;
    albedo -= vec3(dust * 0.22);
    roughness = 0.64 - clamp(v_fabricWear * 0.09, 0.0, 0.14);
    sheen = 0.12 + clamp(v_bodyHeight * 0.05, 0.0, 0.07);
    ao *= 1.0 - dust * 0.32;
    wrap = 0.56;
  } else if (is_leather) {
    float leather_grain = fbm(uv * 8.5) * 0.18;
    float craft_detail = noise(uv * 30.0) * 0.08;
    float stitching = step(0.91, fract(v_worldPos.x * 15.0)) *
                      step(0.91, fract(v_worldPos.y * 13.0)) * 0.09;
    float edge_wear =
        smoothstep(0.85, 0.94, abs(dot(N, normalize(T + B)))) * 0.09;
    N_used = perturb_leather_normal(N, T, B, uv);
    vec3 leather_base = vec3(0.46, 0.32, 0.20);
    albedo = leather_base;
    float belt_band = smoothstep(0.47, 0.49, v_bodyHeight) -
                      smoothstep(0.53, 0.55, v_bodyHeight);
    albedo *= 1.08 + leather_grain + craft_detail - 0.03;
    albedo += vec3(stitching + edge_wear);
    albedo = mix(albedo, mix(albedo, teamColor, 0.82), belt_band);
    roughness = 0.50 - clamp(v_fabricWear * 0.06, 0.0, 0.11);
    sheen = 0.13;
    wrap = 0.48;
  } else if (is_tools) {
    float patina = noise(uv * 15.0) * 0.17 + fbm(uv * 24.0) * 0.11;
    float edge_polish =
        smoothstep(0.85, 0.96, abs(dot(N, normalize(T + B)))) * 0.16;
    N_used = perturb_bronze_normal(N, T, B, uv);
    vec3 bronze_default = vec3(0.80, 0.60, 0.34);
    float custom_weight =
        clamp(max(max(base_color.r, base_color.g), base_color.b), 0.0, 1.0);
    vec3 bronze_base = mix(bronze_default, base_color, custom_weight);
    bronze_base = max(bronze_base, vec3(0.02));
    albedo = bronze_base;
    albedo -= vec3(patina * 0.20);
    albedo += vec3(edge_polish);
    roughness = 0.28 + patina * 0.11;
    metallic = 0.94;
    sheen = 0.18;
    wrap = 0.40;
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
