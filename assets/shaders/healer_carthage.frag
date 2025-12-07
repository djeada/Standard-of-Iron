#version 330 core

// ============================================================================
// CARTHAGINIAN/PHOENICIAN HEALER SHADER
// Mediterranean linen with Tyrian purple trim, leather craft, bronze tools,
// and groomed beard shading focused on natural materials and soft cloth light
// ============================================================================

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

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

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

// ============================================================================
// MATERIAL DETAIL
// ============================================================================

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

float tyrian_dye_variation(vec2 p) {
  float base_variation = noise(p * 5.5) * 0.22;
  float marbling = fbm(p * 10.0) * 0.12;
  float shellfish_pattern = noise(p * 18.0) * 0.06;
  return base_variation + marbling + shellfish_pattern;
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

// ============================================================================
// LIGHTING HELPERS
// ============================================================================

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
  vec3 sky = vec3(0.62, 0.74, 0.88);
  vec3 ground = vec3(0.38, 0.32, 0.26);
  return sky * (0.28 + 0.50 * up) + ground * (0.12 + 0.32 * down);
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

  float ao_strength = mix(0.35, 1.0, clamp(ao, 0.0, 1.0));
  return ambient * (0.55 + 0.45 * ao_strength) + light * ao_strength;
}

// ============================================================================
// BEARD/FACIAL HAIR RENDERING
// ============================================================================

float beard_density(vec2 uv, vec3 worldPos) {
  float strand_base = fbm(uv * 24.0) * 0.6;
  float curl_pattern = sin(uv.x * 80.0 + noise(uv * 40.0) * 3.0) * 0.2;
  float density_variation = noise(uv * 25.0) * 0.4;
  float jaw_bias = smoothstep(1.36, 1.60, worldPos.y) * 0.25;
  return strand_base + curl_pattern + density_variation + jaw_bias;
}

vec3 apply_beard_shading(vec3 base_skin, vec2 uv, vec3 normal, vec3 worldPos,
                         vec3 V, vec3 L) {
  vec3 beard_color = vec3(0.10, 0.07, 0.05);

  float density = beard_density(uv, worldPos);

  float chin_mask = smoothstep(1.55, 1.43, worldPos.y);
  float jawline = smoothstep(1.48, 1.36, worldPos.y);
  float beard_mask = clamp(chin_mask * 0.7 + jawline * 0.45, 0.0, 1.0);

  float strand_highlight = pow(noise(uv * 220.0), 2.2) * 0.16;
  float anisotropic =
      pow(1.0 - abs(dot(normalize(normal + L * 0.28), V)), 7.0) * 0.10;
  beard_color += vec3(strand_highlight + anisotropic);

  return mix(base_skin, beard_color, density * beard_mask * 0.85);
}

// ============================================================================
// MAIN FRAGMENT SHADER
// ============================================================================

void main() {
  vec3 base_color = u_color;
  if (u_useTexture) {
    base_color *= texture(u_texture, v_texCoord).rgb;
  }

  vec3 N = normalize(v_worldNormal);
  vec3 T = normalize(v_tangent);
  vec3 B = normalize(v_bitangent);
  vec2 uv = v_worldPos.xz * 4.5;
  float avg_color = (base_color.r + base_color.g + base_color.b) / 3.0;

  // Material ID: 0=body/skin, 1=tunic/robe, 2=purple trim, 3=leather, 4=tools
  bool is_body = (u_materialId == 0);
  bool is_tunic = (u_materialId == 1);
  bool is_purple_trim = (u_materialId == 2);
  bool is_leather = (u_materialId == 3);
  bool is_tools = (u_materialId == 4);

  // Fallback detection only if no material id provided
  bool has_material_id = (u_materialId >= 0);
  bool looks_light = (!has_material_id) && (avg_color > 0.72);
  bool looks_purple =
      (!has_material_id) && (base_color.b > base_color.g * 1.12 &&
                             base_color.b > base_color.r * 1.05);
  bool looks_skin =
      (!has_material_id) && (avg_color > 0.45 && avg_color < 0.72 &&
                             base_color.r > base_color.g * 0.95 &&
                             base_color.r > base_color.b * 1.05);

  vec3 V = normalize(vec3(-0.2, 1.0, 0.35));
  vec3 L = normalize(vec3(1.0, 1.30, 0.8));

  float curvature = length(dFdx(N)) + length(dFdy(N));
  float ao_folds =
      clamp(1.0 - (v_clothFolds * 0.55 + curvature * 0.80), 0.25, 1.0);
  float dust_mask = smoothstep(0.22, 0.0, v_bodyHeight);
  float sun_bleach = smoothstep(0.55, 1.05, v_bodyHeight) * 0.07;

  vec3 albedo = base_color;
  vec3 N_used = N;
  float roughness = 0.55;
  float metallic = 0.0;
  float sheen = 0.0;
  float wrap = 0.44;
  float ao = ao_folds;

  // === CARTHAGINIAN HEALER MATERIALS ===
  if (is_tunic || looks_light) {
    float linen = phoenician_linen(uv);
    float weave = cloth_weave(uv);
    float drape_folds = v_clothFolds * noise(uv * 9.0) * 0.18;
    float dust = dust_mask * (0.12 + noise(uv * 7.0) * 0.12);

    N_used = perturb_cloth_normal(N, T, B, uv, 128.0, 116.0, 0.08);

    albedo = mix(base_color, vec3(0.93, 0.89, 0.82), 0.55);
    albedo *= 1.0 + linen + weave * 0.08 - drape_folds;
    albedo += vec3(0.02, 0.015, 0.0) * sun_bleach;
    albedo -= vec3(dust * 0.25);

    roughness = 0.72 - clamp(v_fabricWear * 0.08, 0.0, 0.12);
    sheen = 0.08 + clamp(v_bodyHeight * 0.04, 0.0, 0.06);
    ao *= 1.0 - dust * 0.30;
    wrap = 0.54;
  } else if (is_purple_trim || looks_purple) {
    float dye = tyrian_dye_variation(uv);
    float silk = noise(uv * 52.0) * 0.06;
    float thread_ridge = cloth_weave(uv * 1.1);

    N_used = perturb_cloth_normal(N, T, B, uv, 150.0, 142.0, 0.05);

    albedo = mix(base_color, vec3(0.32, 0.10, 0.44), 0.40);
    albedo *= 1.0 + dye + silk + thread_ridge;
    albedo += vec3(0.03, 0.0, 0.05) * clamp(dot(N, V), 0.0, 1.0);

    roughness = 0.42;
    sheen = 0.16;
    metallic = 0.05;
    wrap = 0.48;
  } else if (is_leather || (avg_color > 0.28 && avg_color <= 0.52)) {
    float leather_grain = fbm(uv * 8.0) * 0.16;
    float craft_detail = noise(uv * 28.0) * 0.07;
    float stitching = step(0.92, fract(v_worldPos.x * 14.0)) *
                      step(0.92, fract(v_worldPos.y * 12.0)) * 0.08;
    float edge_wear =
        smoothstep(0.86, 0.94, abs(dot(N, normalize(T + B)))) * 0.08;

    N_used = perturb_leather_normal(N, T, B, uv);

    albedo = mix(base_color, vec3(0.44, 0.30, 0.18), 0.20);
    albedo *= 1.0 + leather_grain + craft_detail - 0.04;
    albedo += vec3(stitching + edge_wear);

    roughness = 0.55 - clamp(v_fabricWear * 0.05, 0.0, 0.10);
    sheen = 0.10;
    wrap = 0.46;
  } else if (is_body || looks_skin) {
    float skin_detail = noise(uv * 24.0) * 0.06;
    float subdermal = noise(uv * 7.0) * 0.05;

    N_used = normalize(N + vec3(0.0, 0.01, 0.0) *
                               triplanar_noise(v_worldPos * 3.0, N, 5.5));

    albedo *= 1.0 + skin_detail;
    albedo += vec3(0.03, 0.015, 0.0) * subdermal;

    bool is_face_region = (v_worldPos.y > 1.40 && v_worldPos.y < 1.65);
    if (is_face_region) {
      albedo = apply_beard_shading(albedo, uv, N_used, v_worldPos, V, L);
    }

    float rim = pow(1.0 - clamp(dot(N_used, V), 0.0, 1.0), 4.0) * 0.05;
    albedo += vec3(rim);

    roughness = 0.55;
    sheen = 0.06 + subdermal * 0.2;
    wrap = 0.46;
  } else if (is_tools) {
    float patina = noise(uv * 14.0) * 0.15 + fbm(uv * 22.0) * 0.10;
    float edge_polish =
        smoothstep(0.86, 0.95, abs(dot(N, normalize(T + B)))) * 0.14;

    N_used = perturb_bronze_normal(N, T, B, uv);

    albedo = mix(base_color, vec3(0.72, 0.52, 0.28), 0.65);
    albedo -= vec3(patina * 0.24);
    albedo += vec3(edge_polish);

    roughness = 0.30 + patina * 0.12;
    metallic = 0.92;
    sheen = 0.12;
    wrap = 0.42;
  } else {
    float detail = noise(uv * 12.0) * 0.10;
    albedo *= 1.0 + detail - 0.05;
  }

  vec3 color = apply_lighting(albedo, N_used, V, L, roughness, metallic, ao,
                              sheen, wrap);
  FragColor = vec4(clamp(color, 0.0, 1.0), u_alpha);
}
