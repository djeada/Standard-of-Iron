#version 330 core

// ============================================================================
// ROMAN MEDICUS (HEALER) SHADER
// Clean, practical Roman medical professional appearance
// Features: White linen tunica, red wool sash/trim, leather equipment
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
  return weave + fine_thread;
}

float roman_wool(vec2 p) {
  // Coarser wool for cape/sash - more texture
  float coarse_weave = sin(p.x * 55.0) * sin(p.y * 52.0) * 0.08;
  float fiber_variation = noise(p * 65.0) * 0.09;
  return coarse_weave + fiber_variation;
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
  float avg_color = (color.r + color.g + color.b) / 3.0;

  // Material ID: 0=body/skin, 1=tunica, 2=leather, 3=medical tools, 4=red trim/sash
  bool is_body = (u_materialId == 0);
  bool is_tunica = (u_materialId == 1);
  bool is_leather = (u_materialId == 2);
  bool is_medical_tools = (u_materialId == 3);
  bool is_red_trim = (u_materialId == 4);

  // Fallback detection for compatibility
  bool looks_light = (avg_color > 0.75);
  bool looks_red = (color.r > color.g * 1.8 && color.r > color.b * 2.0);
  bool looks_brown = (avg_color > 0.25 && avg_color < 0.55 && color.r > color.b);

  // === ROMAN MEDICUS MATERIALS ===

  // WHITE/CREAM LINEN TUNICA (main garment - bleached Roman style)
  if (is_tunica || looks_light) {
    // Fine linen weave with crisp Roman finish
    float linen = roman_linen(v_worldPos.xz);
    float fine_thread = noise(uv * 98.0) * 0.05;

    // Cloth folds from vertex shader (practical Roman draping)
    float fold_depth = v_clothFolds * noise(uv * 14.0) * 0.16;

    // Wear patterns (knees, elbows - working medicus)
    float wear_pattern = v_fabricWear * noise(uv * 9.0) * 0.11;

    // Bleached linen has subtle sheen (different from unbleached)
    vec3 V = normalize(vec3(0.0, 1.0, 0.2));
    float view_angle = max(dot(normalize(v_worldNormal), V), 0.0);
    float linen_sheen = pow(1.0 - view_angle, 11.0) * 0.14;

    // Slight yellowing from age/sweat (practical garment)
    float aging = noise(uv * 2.5) * 0.06;
    vec3 age_tint = vec3(0.02, 0.01, -0.01) * aging;

    color *= 1.0 + linen + fine_thread - 0.025;
    color -= vec3(fold_depth + wear_pattern);
    color += vec3(linen_sheen);
    color += age_tint;
  }
  // RED WOOL SASH/TRIM (military medicus identification)
  else if (is_red_trim || looks_red) {
    // Wool weave (coarser than linen)
    float weave = roman_wool(v_worldPos.xz);
    float wool_tex = noise(uv * 58.0) * 0.10;

    // Madder root dye - natural variation in richness
    float dye_richness = noise(uv * 4.5) * 0.15;
    float dye_depth = noise(uv * 8.0) * 0.08;

    // Wool has matte finish, slight fuzz
    vec3 V = normalize(vec3(0.0, 1.0, 0.3));
    float view_angle = max(dot(normalize(v_worldNormal), V), 0.0);
    float wool_sheen = pow(1.0 - view_angle, 6.0) * 0.09;

    // Fading from sun/washing (outer garment)
    float sun_fading = smoothstep(0.75, 1.0, v_bodyHeight) * 0.07;
    float wash_fade = noise(uv * 3.0) * 0.05;

    color *= 1.0 + weave + wool_tex + dye_richness + dye_depth - 0.04;
    color += vec3(wool_sheen);
    color -= vec3(sun_fading + wash_fade);
  }
  // LEATHER EQUIPMENT (medical bag, belt, sandals, straps)
  else if (is_leather || looks_brown) {
    // Vegetable-tanned Roman leather
    float leather_grain = noise(uv * 16.0) * 0.16 * (1.0 + v_fabricWear * 0.25);
    float pores = noise(uv * 38.0) * 0.06;

    // Roman tooling and stitching
    float tooling = noise(uv * 24.0) * 0.05;
    float stitching = step(0.94, fract(v_worldPos.x * 16.0)) *
                      step(0.94, fract(v_worldPos.y * 14.0)) * 0.07;

    // Well-maintained leather (medical professional)
    float oil_darkening = v_fabricWear * 0.12;
    float conditioning = noise(uv * 6.0) * 0.04;

    // Subtle leather sheen
    vec3 V = normalize(vec3(0.0, 1.0, 0.4));
    float view_angle = max(dot(normalize(v_worldNormal), V), 0.0);
    float leather_sheen = pow(1.0 - view_angle, 5.0) * 0.12;

    // Edge wear (lighter at stressed edges)
    float edge_wear =
        smoothstep(0.86, 0.92, abs(dot(normal, v_tangent))) * 0.09;

    color *= 1.0 + leather_grain + pores + tooling + conditioning - oil_darkening;
    color += vec3(stitching + leather_sheen + edge_wear);
  }
  // BRONZE MEDICAL IMPLEMENTS
  else if (is_medical_tools) {
    // Roman surgical instruments were typically bronze
    vec3 bronze_base = vec3(0.76, 0.56, 0.32);

    // Patina from use and frequent cleaning
    float patina = noise(uv * 13.0) * 0.16;
    float verdigris = noise(uv * 20.0) * 0.08;

    // Polished from handling (well-used tools)
    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float view_angle = max(dot(normalize(v_worldNormal), V), 0.0);
    float bronze_sheen = pow(view_angle, 9.0) * 0.38;

    // Tool edges are brighter (polished sharp edges)
    float edge_polish = smoothstep(0.85, 0.95, abs(dot(normal, v_tangent))) * 0.12;

    color = mix(color, bronze_base, 0.58);
    color -= vec3(patina * 0.28 + verdigris * 0.18);
    color += vec3(bronze_sheen + edge_polish);
  }
  // BODY/SKIN (Roman - clean-shaven face, hands, arms)
  else if (is_body) {
    // Romans typically clean-shaven - smooth skin
    float skin_detail = noise(uv * 28.0) * 0.07;
    float skin_subsurface = noise(uv * 8.0) * 0.04;
    
    color *= 1.0 + skin_detail;
    // Slight warmth for subsurface scattering effect
    color += vec3(0.02, 0.01, 0.0) * skin_subsurface;
  }
  // DEFAULT (catch-all)
  else {
    float detail = noise(uv * 11.0) * 0.09;
    color *= 1.0 + detail - 0.05;
  }

  color = clamp(color, 0.0, 1.0);

  // Lighting model (soft for cloth-based unit)
  vec3 light_dir = normalize(vec3(1.0, 1.25, 1.0));
  float n_dot_l = dot(normal, light_dir);

  // Cloth wraps light softly
  float wrap_amount = is_tunica ? 0.52 : (is_red_trim ? 0.48 : 0.44);
  float diff = max(n_dot_l * (1.0 - wrap_amount) + wrap_amount, 0.26);

  color *= diff;
  FragColor = vec4(color, u_alpha);
}
