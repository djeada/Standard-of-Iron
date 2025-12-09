#version 330 core

// ============================================================================
// ROMAN MEDICUS (HEALER) SHADER
// Clean, practical Roman medical professional appearance with crisp textiles,
// maintained leather, polished bronze tools, and soft wrap lighting.
// ============================================================================

// ============================================================================
// INPUTS & OUTPUTS
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
    freq *= 2.1;
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
// ROMAN TEXTILE PATTERNS
// ============================================================================

float cloth_weave(vec2 p) {
  // Tight Roman linen weave
  float warp_thread = sin(p.x * 72.0);
  float weft_thread = sin(p.y * 70.0);
  return warp_thread * weft_thread * 0.055;
}

float roman_linen(vec2 p) {
  // Fine bleached Roman linen - crisp and clean
  float weave = cloth_weave(p);
  float fine_thread = noise(p * 95.0) * 0.06;
  float slub = fbm(p * 7.5) * 0.05;
  return weave + fine_thread + slub;
}

float roman_wool(vec2 p) {
  // Coarser wool for cape/sash - more texture
  float coarse_weave = sin(p.x * 55.0) * sin(p.y * 52.0) * 0.08;
  float fiber_variation = noise(p * 65.0) * 0.09;
  float nap = fbm(p * 9.0) * 0.05;
  return coarse_weave + fiber_variation + nap;
}

// ============================================================================
// PERTURBED NORMALS
// ============================================================================

vec3 perturb_linen_normal(vec3 N, vec3 T, vec3 B, vec2 uv) {
  float warp = sin(uv.x * 142.0) * 0.05;
  float weft = sin(uv.y * 138.0) * 0.05;
  float slub = fbm(uv * 7.0) * 0.04;
  return normalize(N + T * (warp + slub) + B * (weft + slub * 0.6));
}

vec3 perturb_wool_normal(vec3 N, vec3 T, vec3 B, vec2 uv) {
  float weave = sin(uv.x * 58.0) * 0.08 + sin(uv.y * 56.0) * 0.08;
  float fuzz = fbm(uv * 12.0) * 0.06;
  return normalize(N + T * (weave + fuzz * 0.6) + B * (weave * 0.6 + fuzz));
}

vec3 perturb_leather_normal(vec3 N, vec3 T, vec3 B, vec2 uv) {
  float grain = fbm(uv * 8.5) * 0.16;
  float pores = noise(uv * 34.0) * 0.10;
  float scars = noise(uv * 16.0 + vec2(2.7, -1.9)) * 0.07;
  return normalize(N + T * (grain + scars * 0.4) + B * (pores + scars * 0.3));
}

vec3 perturb_bronze_normal(vec3 N, vec3 T, vec3 B, vec2 uv) {
  float hammer = fbm(uv * 15.0) * 0.14;
  float ripple = noise(uv * 46.0) * 0.05;
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
  vec3 sky = vec3(0.66, 0.76, 0.90);
  vec3 ground = vec3(0.42, 0.36, 0.30);
  return sky * (0.26 + 0.54 * up) + ground * (0.14 + 0.30 * down);
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
  return ambient * (0.56 + 0.44 * ao_strength) + light * ao_strength;
}

// ============================================================================
// BEARD/FACIAL HAIR RENDERING (parity with Carthage healer)
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

  // Material ID: 0=body/skin, 1=tunica, 2=leather, 3=medical tools, 4=red
  // trim/sash
  bool is_body = (u_materialId == 0);
  bool is_tunica = (u_materialId == 1);
  bool is_leather = (u_materialId == 2);
  bool is_medical_tools = (u_materialId == 3);
  bool is_red_trim = (u_materialId == 4);

  vec3 albedo = base_color;
  vec3 N_used = N;
  float roughness = 0.55;
  float metallic = 0.0;
  float sheen = 0.0;
  float wrap = 0.44;

  vec3 V = normalize(vec3(-0.25, 1.0, 0.4));
  vec3 L = normalize(vec3(1.0, 1.25, 1.0));

  float curvature = length(dFdx(N)) + length(dFdy(N));
  float ao_folds =
      clamp(1.0 - (v_clothFolds * 0.52 + curvature * 0.78), 0.28, 1.0);
  float ao = ao_folds;

  // BODY / SKIN
  if (is_body) {
    vec3 skin = base_color;
    float skin_detail = noise(uv * 24.0) * 0.06;
    float subdermal = noise(uv * 7.0) * 0.05;
    skin *= 1.0 + skin_detail;
    skin += vec3(0.03, 0.015, 0.0) * subdermal;

    bool is_face_region = (v_worldPos.y > 1.40 && v_worldPos.y < 1.65);
    if (is_face_region) {
      skin = apply_beard_shading(skin, uv, N_used, v_worldPos, V, L);
    }

    float rim = pow(1.0 - clamp(dot(N_used, V), 0.0, 1.0), 4.0) * 0.05;
    skin += vec3(rim);

    albedo = skin;
    roughness = 0.55;
    sheen = 0.06 + subdermal * 0.2;
    wrap = 0.46;
  }
  // WHITE/CREAM LINEN TUNICA (main garment - bleached Roman style)
  else if (is_tunica) {
    vec3 tunic_base = vec3(0.95, 0.93, 0.90);
    albedo = tunic_base;

    float linen = roman_linen(v_worldPos.xz);
    float fine_thread = noise(uv * 98.0) * 0.05;

    float fold_depth = v_clothFolds * noise(uv * 14.0) * 0.16;
    float wear_pattern = v_fabricWear * noise(uv * 9.0) * 0.11;

    float dust =
        smoothstep(0.24, 0.0, v_bodyHeight) * (0.10 + noise(uv * 6.5) * 0.10);

    N_used = perturb_linen_normal(N, T, B, uv);

    float view_angle = max(dot(N_used, V), 0.0);
    float linen_sheen = pow(1.0 - view_angle, 11.0) * 0.14;

    float aging = noise(uv * 2.5) * 0.06;
    vec3 age_tint = vec3(0.02, 0.01, -0.01) * aging;

    albedo *= 1.0 + linen + fine_thread - 0.025;
    albedo -= vec3(fold_depth + wear_pattern);
    albedo += vec3(linen_sheen);
    albedo += age_tint;
    albedo -= vec3(dust * 0.18);

    roughness = 0.70 - clamp(v_fabricWear * 0.08, 0.0, 0.12);
    sheen = 0.08;
    wrap = 0.54;
    ao *= 1.0 - dust * 0.35;
  }
  // RED WOOL SASH/TRIM (military medicus identification)
  else if (is_red_trim) {
    float weave = roman_wool(v_worldPos.xz);
    float wool_tex = noise(uv * 58.0) * 0.10;

    float dye_richness = noise(uv * 4.5) * 0.15;
    float dye_depth = noise(uv * 8.0) * 0.08;

    float view_angle = max(dot(N, V), 0.0);
    float wool_sheen = pow(1.0 - view_angle, 6.0) * 0.09;

    float sun_fading = smoothstep(0.75, 1.0, v_bodyHeight) * 0.07;
    float wash_fade = noise(uv * 3.0) * 0.05;

    N_used = perturb_wool_normal(N, T, B, uv);

    albedo = mix(base_color, vec3(0.75, 0.12, 0.12), 0.35);
    albedo *= 1.0 + weave + wool_tex + dye_richness + dye_depth - 0.04;
    albedo += vec3(wool_sheen);
    albedo -= vec3(sun_fading + wash_fade);

    roughness = 0.50;
    sheen = 0.10;
    wrap = 0.48;
  }
  // LEATHER EQUIPMENT (medical bag, belt, sandals, straps)
  else if (is_leather) {
    float leather_grain = noise(uv * 16.0) * 0.16 * (1.0 + v_fabricWear * 0.25);
    float pores = noise(uv * 38.0) * 0.06;

    float tooling = noise(uv * 24.0) * 0.05;
    float stitching = step(0.94, fract(v_worldPos.x * 16.0)) *
                      step(0.94, fract(v_worldPos.y * 14.0)) * 0.07;

    float oil_darkening = v_fabricWear * 0.12;
    float conditioning = noise(uv * 6.0) * 0.04;

    float view_angle = max(dot(N, V), 0.0);
    float leather_sheen = pow(1.0 - view_angle, 5.0) * 0.12;

    float edge_wear = smoothstep(0.86, 0.92, abs(dot(N, T))) * 0.09;

    N_used = perturb_leather_normal(N, T, B, uv);

    albedo *=
        1.0 + leather_grain + pores + tooling + conditioning - oil_darkening;
    albedo += vec3(stitching + leather_sheen + edge_wear);

    roughness = 0.56 - clamp(v_fabricWear * 0.06, 0.0, 0.10);
    sheen = 0.08;
    wrap = 0.46;
  }
  // BRONZE MEDICAL IMPLEMENTS
  else if (is_medical_tools) {
    vec3 bronze_base = vec3(0.76, 0.56, 0.32);

    float patina = noise(uv * 13.0) * 0.16;
    float verdigris = noise(uv * 20.0) * 0.08;

    float view_angle = max(dot(N, V), 0.0);
    float bronze_sheen = pow(view_angle, 9.0) * 0.38;

    float edge_polish = smoothstep(0.85, 0.95, abs(dot(N, T))) * 0.12;

    N_used = perturb_bronze_normal(N, T, B, uv);

    albedo = mix(base_color, bronze_base, 0.58);
    albedo -= vec3(patina * 0.28 + verdigris * 0.18);
    albedo += vec3(bronze_sheen + edge_polish);

    roughness = 0.32 + patina * 0.12;
    metallic = 0.92;
    sheen = 0.12;
    wrap = 0.42;
  }
  // BODY/SKIN (Roman - clean-shaven face, hands, arms)
  else if (is_body) {
    float skin_detail = noise(uv * 28.0) * 0.07;
    float skin_subsurface = noise(uv * 8.0) * 0.04;

    N_used = normalize(N + vec3(0.0, 0.01, 0.0) *
                               triplanar_noise(v_worldPos * 3.0, N, 5.5));

    albedo *= 1.0 + skin_detail;
    albedo += vec3(0.02, 0.01, 0.0) * skin_subsurface;

    float rim = pow(1.0 - clamp(dot(N_used, V), 0.0, 1.0), 4.0) * 0.04;
    albedo += vec3(rim);

    roughness = 0.54;
    sheen = 0.05;
    wrap = 0.46;
  }
  // DEFAULT (catch-all)
  else {
    float detail = noise(uv * 11.0) * 0.09;
    albedo *= 1.0 + detail - 0.05;
  }

  vec3 color = apply_lighting(albedo, N_used, V, L, roughness, metallic, ao,
                              sheen, wrap);
  FragColor = vec4(clamp(color, 0.0, 1.0), u_alpha);
}
