#version 330 core

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

// ============================================================================
// MATERIAL PATTERN FUNCTIONS (Roman Medicus)
// ============================================================================

float cloth_weave(vec2 p) {
  float warp_thread = sin(p.x * 70.0);
  float weft_thread = sin(p.y * 68.0);
  return warp_thread * weft_thread * 0.06;
}

float roman_linen(vec2 p) {
  float weave = cloth_weave(p);
  float fine_thread = noise(p * 90.0) * 0.07;
  return weave + fine_thread;
}

// ============================================================================
// MAIN FRAGMENT SHADER
// ============================================================================

void main() {
  vec3 color = u_color;
  if (u_useTexture) {
    color *= texture(u_texture, v_texCoord).rgb;
  }

  vec3 normal = normalize(v_normal);
  vec2 uv = v_worldPos.xz * 4.5;

  // Material ID: 0=body/skin, 1=tunica, 2=leather, 3=medical tools, 4=red
  // trim/cape
  bool is_body = (u_materialId == 0);
  bool is_tunica = (u_materialId == 1);
  bool is_leather = (u_materialId == 2);
  bool is_medical_tools = (u_materialId == 3);
  bool is_red_trim = (u_materialId == 4);

  // Use material IDs exclusively (no fallbacks)

  // === ROMAN MEDICUS MATERIALS ===

  // WHITE LINEN TUNICA (main garment)
  if (is_tunica) {
    // Fine linen weave with natural texture
    float linen = roman_linen(v_worldPos.xz);
    float fine_thread = noise(uv * 95.0) * 0.06;

    // Cloth folds from vertex shader (natural draping)
    float fold_depth = v_clothFolds * noise(uv * 12.0) * 0.18;

    // Wear patterns on high-stress areas (elbows, knees)
    float wear_pattern = v_fabricWear * noise(uv * 8.0) * 0.12;

    // Natural linen has subtle sheen (different from silk)
    vec3 V = normalize(vec3(0.0, 1.0, 0.2));
    float view_angle = max(dot(normalize(v_worldNormal), V), 0.0);
    float linen_sheen = pow(1.0 - view_angle, 10.0) * 0.12;

    // Slight discoloration from use (natural aging)
    float aging = noise(uv * 3.0) * 0.08;

    color *= 1.0 + linen + fine_thread - 0.03;
    color -= vec3(fold_depth + wear_pattern + aging);
    color += vec3(linen_sheen);
  }
  // RED WOOL CAPE/TRIM (military medicus insignia)
  else if (is_red_trim) {
    // Wool weave (coarser than linen)
    float weave = cloth_weave(v_worldPos.xz);
    float wool_tex = noise(uv * 55.0) * 0.10;

    // Natural madder root dye richness variation
    float dye_richness = noise(uv * 5.0) * 0.14;

    // Wool has different sheen than linen (more matte)
    vec3 V = normalize(vec3(0.0, 1.0, 0.3));
    float view_angle = max(dot(normalize(v_worldNormal), V), 0.0);
    float wool_sheen = pow(1.0 - view_angle, 7.0) * 0.11;

    // Fading from sun exposure (outer garment)
    float sun_fading = smoothstep(0.8, 1.0, v_bodyHeight) * 0.08;

    color *= 1.0 + weave + wool_tex + dye_richness - 0.04;
    color += vec3(wool_sheen);
    color -= vec3(sun_fading);
  }
  // LEATHER EQUIPMENT (bag, belt, sandals, straps)
  else if (is_leather) {
    // Vegetable-tanned leather grain
    float leather_grain = noise(uv * 15.0) * 0.15 * (1.0 + v_fabricWear * 0.3);
    float pores = noise(uv * 35.0) * 0.07;

    // Roman tooling and stitching marks
    float tooling = noise(uv * 22.0) * 0.05;
    float stitching = step(0.95, fract(v_worldPos.x * 15.0)) *
                      step(0.95, fract(v_worldPos.y * 12.0)) * 0.08;

    // Leather darkens and stiffens with age/oil
    float oil_darkening = v_fabricWear * 0.15;

    // Subtle leather sheen (not glossy, just slight reflection)
    vec3 V = normalize(vec3(0.0, 1.0, 0.4));
    float view_angle = max(dot(normalize(v_worldNormal), V), 0.0);
    float leather_sheen = pow(1.0 - view_angle, 5.5) * 0.11;

    // Edge wear (lighter color at stressed edges)
    float edge_wear =
        smoothstep(0.88, 0.92, abs(dot(normal, v_tangent))) * 0.10;

    color *= 1.0 + leather_grain + pores + tooling - oil_darkening;
    color += vec3(stitching + leather_sheen + edge_wear);
  }
  // MEDICAL IMPLEMENTS (bronze/iron tools)
  else if (is_medical_tools) {
    // Bronze medical instruments (Roman surgical tools were bronze)
    vec3 bronze_base = vec3(0.75, 0.55, 0.30);

    // Tool patina from use and cleaning
    float patina = noise(uv * 12.0) * 0.18;
    float verdigris = noise(uv * 18.0) * 0.10;

    // Polished areas from frequent handling
    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float view_angle = max(dot(normalize(v_worldNormal), V), 0.0);
    float bronze_sheen = pow(view_angle, 8.0) * 0.35;

    color = mix(color, bronze_base, 0.6);
    color -= vec3(patina * 0.3 + verdigris * 0.2);
    color += vec3(bronze_sheen);
  }
  // BODY/SKIN (hands, face, neck)
  else if (is_body) {
    // Minimal processing for skin - let base color through
    float skin_detail = noise(uv * 25.0) * 0.08;
    color *= 1.0 + skin_detail;
  }

  color = clamp(color, 0.0, 1.0);

  // Lighting model (softer for cloth-based unit)
  vec3 light_dir = normalize(vec3(1.0, 1.2, 1.0));
  float n_dot_l = dot(normal, light_dir);

  float wrap_amount = is_tunica ? 0.50 : (is_red_trim ? 0.48 : 0.42);
  float diff = max(n_dot_l * (1.0 - wrap_amount) + wrap_amount, 0.25);

  color *= diff;
  FragColor = vec4(color, u_alpha);
}
