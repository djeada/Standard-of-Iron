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

float clothWeave(vec2 p) {
  float warpThread = sin(p.x * 70.0);
  float weftThread = sin(p.y * 68.0);
  return warpThread * weftThread * 0.06;
}

float romanLinen(vec2 p) {
  float weave = clothWeave(p);
  float fineThread = noise(p * 90.0) * 0.07;
  return weave + fineThread;
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
  
  // Material ID: 0=body/skin, 1=tunica, 2=leather, 3=medical tools, 4=red trim/cape
  bool isBody = (u_materialId == 0);
  bool isTunica = (u_materialId == 1);
  bool isLeather = (u_materialId == 2);
  bool isMedicalTools = (u_materialId == 3);
  bool isRedTrim = (u_materialId == 4);
  
  // Fallback to old color-based detection if materialId not set
  if (u_materialId == 0) {
    float avgColor = (color.r + color.g + color.b) / 3.0;
    isTunica = (avgColor > 0.72);
    isRedTrim = (color.r > color.g * 1.18 && color.r > color.b * 1.15);
    isLeather = (!isTunica && !isRedTrim && avgColor > 0.32 && avgColor <= 0.58);
  }

  // === ROMAN MEDICUS MATERIALS ===

  // WHITE LINEN TUNICA (main garment)
  if (isTunica) {
    // Fine linen weave with natural texture
    float linen = romanLinen(v_worldPos.xz);
    float fineThread = noise(uv * 95.0) * 0.06;
    
    // Cloth folds from vertex shader (natural draping)
    float foldDepth = v_clothFolds * noise(uv * 12.0) * 0.18;
    
    // Wear patterns on high-stress areas (elbows, knees)
    float wearPattern = v_fabricWear * noise(uv * 8.0) * 0.12;
    
    // Natural linen has subtle sheen (different from silk)
    vec3 V = normalize(vec3(0.0, 1.0, 0.2));
    float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);
    float linenSheen = pow(1.0 - viewAngle, 10.0) * 0.12;
    
    // Slight discoloration from use (natural aging)
    float aging = noise(uv * 3.0) * 0.08;
    
    color *= 1.0 + linen + fineThread - 0.03;
    color -= vec3(foldDepth + wearPattern + aging);
    color += vec3(linenSheen);
  }
  // RED WOOL CAPE/TRIM (military medicus insignia)
  else if (isRedTrim) {
    // Wool weave (coarser than linen)
    float weave = clothWeave(v_worldPos.xz);
    float woolTex = noise(uv * 55.0) * 0.10;
    
    // Natural madder root dye richness variation
    float dyeRichness = noise(uv * 5.0) * 0.14;
    
    // Wool has different sheen than linen (more matte)
    vec3 V = normalize(vec3(0.0, 1.0, 0.3));
    float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);
    float woolSheen = pow(1.0 - viewAngle, 7.0) * 0.11;
    
    // Fading from sun exposure (outer garment)
    float sunFading = smoothstep(0.8, 1.0, v_bodyHeight) * 0.08;

    color *= 1.0 + weave + woolTex + dyeRichness - 0.04;
    color += vec3(woolSheen);
    color -= vec3(sunFading);
  }
  // LEATHER EQUIPMENT (bag, belt, sandals, straps)
  else if (isLeather) {
    // Vegetable-tanned leather grain
    float leatherGrain = noise(uv * 15.0) * 0.15 * (1.0 + v_fabricWear * 0.3);
    float pores = noise(uv * 35.0) * 0.07;
    
    // Roman tooling and stitching marks
    float tooling = noise(uv * 22.0) * 0.05;
    float stitching = step(0.95, fract(v_worldPos.x * 15.0)) * 
                      step(0.95, fract(v_worldPos.y * 12.0)) * 0.08;
    
    // Leather darkens and stiffens with age/oil
    float oilDarkening = v_fabricWear * 0.15;
    
    // Subtle leather sheen (not glossy, just slight reflection)
    vec3 V = normalize(vec3(0.0, 1.0, 0.4));
    float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);
    float leatherSheen = pow(1.0 - viewAngle, 5.5) * 0.11;
    
    // Edge wear (lighter color at stressed edges)
    float edgeWear = smoothstep(0.88, 0.92, abs(dot(normal, v_tangent))) * 0.10;

    color *= 1.0 + leatherGrain + pores + tooling - oilDarkening;
    color += vec3(stitching + leatherSheen + edgeWear);
  }
  // MEDICAL IMPLEMENTS (bronze/iron tools)
  else if (isMedicalTools) {
    // Bronze medical instruments (Roman surgical tools were bronze)
    vec3 bronzeBase = vec3(0.75, 0.55, 0.30);
    
    // Tool patina from use and cleaning
    float patina = noise(uv * 12.0) * 0.18;
    float verdigris = noise(uv * 18.0) * 0.10;
    
    // Polished areas from frequent handling
    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);
    float bronzeSheen = pow(viewAngle, 8.0) * 0.35;
    
    color = mix(color, bronzeBase, 0.6);
    color -= vec3(patina * 0.3 + verdigris * 0.2);
    color += vec3(bronzeSheen);
  }
  // BODY/SKIN (hands, face, neck)
  else if (isBody) {
    // Minimal processing for skin - let base color through
    float skinDetail = noise(uv * 25.0) * 0.08;
    color *= 1.0 + skinDetail;
  }

  color = clamp(color, 0.0, 1.0);

  // Lighting model (softer for cloth-based unit)
  vec3 lightDir = normalize(vec3(1.0, 1.2, 1.0));
  float nDotL = dot(normal, lightDir);
  
  float wrapAmount = isTunica ? 0.50 : (isRedTrim ? 0.48 : 0.42);
  float diff = max(nDotL * (1.0 - wrapAmount) + wrapAmount, 0.25);

  color *= diff;
  FragColor = vec4(color, u_alpha);
}
