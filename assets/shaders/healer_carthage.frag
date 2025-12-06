#version 330 core

// ============================================================================
// CARTHAGINIAN/PHOENICIAN HEALER SHADER
// Distinct Mediterranean healer with Eastern influences
// Features: Natural linen robes, Tyrian purple trim, beard rendering
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

float hash3(vec3 p) {
  p = fract(p * vec3(0.1031, 0.1030, 0.0973));
  p += dot(p, p.yxz + 33.33);
  return fract((p.x + p.y) * p.z);
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
// PHOENICIAN TEXTILE PATTERNS
// ============================================================================

float cloth_weave(vec2 p) {
  // Looser weave than Roman - more flowing Mediterranean style
  float warp_thread = sin(p.x * 62.0);
  float weft_thread = sin(p.y * 60.0);
  return warp_thread * weft_thread * 0.07;
}

float phoenician_linen(vec2 p) {
  // Fine Phoenician linen - famous across Mediterranean
  float weave = cloth_weave(p);
  float fine_thread = noise(p * 85.0) * 0.08;
  // Subtle pattern embroidery (Eastern influence)
  float embroidery = step(0.92, sin(p.x * 25.0) * sin(p.y * 25.0)) * 0.06;
  return weave + fine_thread + embroidery;
}

float tyrian_dye_variation(vec2 p) {
  // Tyrian purple - natural dye has organic variations
  float base_variation = noise(p * 4.0) * 0.18;
  float shellfish_pattern = noise(p * 12.0) * 0.08;
  return base_variation + shellfish_pattern;
}

// ============================================================================
// BEARD/FACIAL HAIR RENDERING
// ============================================================================

float beard_density(vec2 uv, vec3 worldPos) {
  // Hair strand pattern - short curly Mediterranean beard style
  float strand_base = noise(uv * 120.0) * 0.6;
  float curl_pattern = sin(uv.x * 80.0 + noise(uv * 40.0) * 3.0) * 0.2;
  float density_variation = noise(uv * 25.0) * 0.4;
  return strand_base + curl_pattern + density_variation;
}

vec3 apply_beard_shading(vec3 base_skin, vec2 uv, vec3 normal, vec3 worldPos) {
  // Phoenician/Semitic beard - dark, well-groomed
  vec3 beard_color = vec3(0.08, 0.06, 0.05); // Near-black beard
  
  float density = beard_density(uv, worldPos);
  
  // Beard typically fuller on chin and jawline
  float chin_mask = smoothstep(1.55, 1.45, worldPos.y);
  float jawline = smoothstep(1.48, 1.42, worldPos.y);
  float beard_mask = chin_mask * (0.7 + jawline * 0.3);
  
  // Individual hair strand highlights
  float strand_highlight = pow(noise(uv * 200.0), 2.0) * 0.15;
  beard_color += vec3(strand_highlight);
  
  // Subtle sheen on well-oiled beard (Eastern grooming tradition)
  float view_angle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.3))));
  float beard_sheen = pow(1.0 - view_angle, 8.0) * 0.08;
  beard_color += vec3(beard_sheen);
  
  return mix(base_skin, beard_color, density * beard_mask * 0.85);
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

  // Material ID: 0=body/skin, 1=tunic/robe, 2=purple trim, 3=leather, 4=tools
  bool is_body = (u_materialId == 0);
  bool is_tunic = (u_materialId == 1);
  bool is_purple_trim = (u_materialId == 2);
  bool is_leather = (u_materialId == 3);
  bool is_tools = (u_materialId == 4);

  // Fallback detection for older material systems
  bool looks_light = (avg_color > 0.72);
  bool looks_purple = (color.b > color.g * 1.12 && color.b > color.r * 1.05);
  bool looks_skin = (avg_color > 0.45 && avg_color < 0.72 && 
                     color.r > color.g * 0.95 && color.r > color.b * 1.05);

  // === CARTHAGINIAN HEALER MATERIALS ===

  // NATURAL LINEN ROBE (cream/off-white - undyed Phoenician linen)
  if (is_tunic || looks_light) {
    float linen = phoenician_linen(v_worldPos.xz);
    
    // Flowing Mediterranean draping
    float drape_folds = noise(uv * 7.0) * 0.14;
    float gather_points = noise(uv * 3.5) * 0.08;
    
    // Natural aging and use wear
    float wear = noise(uv * 5.0) * 0.10;
    
    // Linen sheen - subtle but present
    float view_angle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.2))));
    float linen_sheen = pow(1.0 - view_angle, 9.0) * 0.14;
    
    // Slight warmth in shadows (reflected light from skin/ground)
    vec3 shadow_tint = vec3(0.02, 0.01, 0.0);

    color *= 1.0 + linen - 0.02;
    color -= vec3(drape_folds + gather_points + wear);
    color += vec3(linen_sheen);
    color += shadow_tint * (1.0 - view_angle);
  }
  // TYRIAN PURPLE TRIM/SASH (prestigious Phoenician dye)
  else if (is_purple_trim || looks_purple) {
    float weave = cloth_weave(v_worldPos.xz);
    float dye_variation = tyrian_dye_variation(uv);
    
    // Fine silk-like texture for expensive trim
    float silk_texture = noise(uv * 45.0) * 0.05;
    
    // Tyrian purple has distinctive shimmer
    float view_angle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.3))));
    float purple_shimmer = pow(1.0 - view_angle, 5.5) * 0.16;
    
    // Color shift in different lighting (characteristic of murex dye)
    vec3 color_shift = vec3(-0.02, 0.0, 0.03) * view_angle;

    color *= 1.0 + weave + dye_variation + silk_texture - 0.03;
    color += vec3(purple_shimmer);
    color += color_shift;
  }
  // LEATHER (sandals, belt, medical bag)
  else if (is_leather || (avg_color > 0.28 && avg_color <= 0.52)) {
    // North African/Phoenician leather craft
    float leather_grain = noise(uv * 16.0) * 0.16;
    float craft_detail = noise(uv * 28.0) * 0.07;
    
    // Tooled patterns (Phoenician decorative tradition)
    float tooling = step(0.88, sin(uv.x * 20.0) * sin(uv.y * 18.0)) * 0.08;
    
    // Oil-treated leather sheen
    float view_angle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.4))));
    float leather_sheen = pow(1.0 - view_angle, 5.5) * 0.12;
    
    // Edge wear from use
    float edge_wear = smoothstep(0.85, 0.92, abs(dot(normal, vec3(1.0, 0.0, 0.0)))) * 0.08;

    color *= 1.0 + leather_grain + craft_detail - 0.04;
    color += vec3(tooling + leather_sheen + edge_wear);
  }
  // BODY/SKIN with potential beard
  else if (is_body || looks_skin) {
    // Mediterranean skin tone detail
    float skin_detail = noise(uv * 22.0) * 0.07;
    color *= 1.0 + skin_detail;
    
    // Check if this is face/chin region for beard
    bool is_face_region = (v_worldPos.y > 1.40 && v_worldPos.y < 1.65);
    if (is_face_region) {
      color = apply_beard_shading(color, uv, normal, v_worldPos);
    }
  }
  // MEDICAL TOOLS (bronze implements)
  else if (is_tools) {
    vec3 bronze_base = vec3(0.72, 0.52, 0.28);
    float patina = noise(uv * 14.0) * 0.15;
    float view_angle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
    float bronze_sheen = pow(view_angle, 7.0) * 0.30;
    
    color = mix(color, bronze_base, 0.55);
    color -= vec3(patina * 0.25);
    color += vec3(bronze_sheen);
  }
  // DEFAULT (catch-all)
  else {
    float detail = noise(uv * 12.0) * 0.10;
    color *= 1.0 + detail - 0.05;
  }

  color = clamp(color, 0.0, 1.0);

  // Soft lighting for cloth-heavy unit (Mediterranean sunlight feel)
  vec3 light_dir = normalize(vec3(1.0, 1.3, 0.8));
  float n_dot_l = dot(normal, light_dir);
  
  // More wrap for soft cloth materials
  float wrap_amount = is_tunic ? 0.52 : (is_purple_trim ? 0.48 : 0.44);
  float diff = max(n_dot_l * (1.0 - wrap_amount) + wrap_amount, 0.26);

  color *= diff;
  FragColor = vec4(color, u_alpha);
}
