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
in float v_helmetDetail;
in float v_chainmailPhase;
in float v_leatherWear;
in float v_curvature;

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
// MATERIAL PATTERN FUNCTIONS (Roman Archer - Sagittarius)
// ============================================================================

// Roman chainmail (lorica hamata) ring pattern
float chainmailRings(vec2 p) {
  vec2 grid = fract(p * 32.0) - 0.5;
  float ring = length(grid);
  float ringPattern =
      smoothstep(0.38, 0.32, ring) - smoothstep(0.28, 0.22, ring);

  // Offset rows for interlocking
  vec2 offsetGrid = fract(p * 32.0 + vec2(0.5, 0.0)) - 0.5;
  float offsetRing = length(offsetGrid);
  float offsetPattern =
      smoothstep(0.38, 0.32, offsetRing) - smoothstep(0.28, 0.22, offsetRing);

  return (ringPattern + offsetPattern) * 0.14;
}

// Leather pteruges strips (hanging skirt/shoulder guards)
float pterugesStrips(vec2 p, float y) {
  // Vertical leather strips
  float stripX = fract(p.x * 9.0);
  float strip = smoothstep(0.15, 0.20, stripX) - smoothstep(0.80, 0.85, stripX);

  // Add leather texture to strips
  float leatherTex = noise(p * 18.0) * 0.35;

  // Strips hang and curve
  float hang = smoothstep(0.65, 0.45, y);

  return strip * leatherTex * hang;
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
  
  // Material ID: 0=body/skin, 1=armor, 2=helmet, 3=weapon, 4=shield
  bool isArmor = (u_materialId == 1);
  bool isHelmet = (u_materialId == 2);
  bool isWeapon = (u_materialId == 3);
  bool isShield = (u_materialId == 4);
  
  // Fallback to old layer system for non-armor meshes
  if (u_materialId == 0) {
    isHelmet = (v_armorLayer < 0.5);
    isArmor = false;  // Body mesh should not get armor effects
  }
  
  bool isLegs = (v_armorLayer >= 1.5);

  // === ROMAN ARCHER (SAGITTARIUS) MATERIALS ===

  // LIGHT BRONZE HELMET (warm golden auxiliary helmet)
  if (isHelmet) {
    // Use vertex-computed helmet detail
    float bands = v_helmetDetail * 0.15;
    
    // Warm bronze patina and wear
    float bronzePatina = noise(uv * 8.0) * 0.12;
    float verdigris = noise(uv * 15.0) * 0.08;
    
    // Hammer marks from forging (using vertex curvature)
    float hammerMarks = noise(uv * 25.0) * 0.035 * (1.0 - v_curvature * 0.3);
    
    // Conical shape highlight
    float apex = smoothstep(0.85, 1.0, v_bodyHeight) * 0.12;
    
    // ENHANCED: Cheek guards (hinged bronze plates)
    float cheekGuardHeight = smoothstep(0.72, 0.82, v_bodyHeight) * 
                             smoothstep(0.88, 0.82, v_bodyHeight);
    float cheekX = abs(v_worldPos.x);
    float cheekGuard = cheekGuardHeight * smoothstep(0.10, 0.08, cheekX) * 0.35;
    float guardEdge = cheekGuard * step(0.32, noise(uv * 18.0)) * 0.18;
    
    // ENHANCED: Neck guard (rear projection)
    float neckGuardHeight = smoothstep(0.68, 0.74, v_bodyHeight) * 
                            smoothstep(0.80, 0.74, v_bodyHeight);
    float behindHead = step(v_worldNormal.z, -0.3);  // Rear-facing normals
    float neckGuard = neckGuardHeight * behindHead * 0.28;
    float neckSegments = fract(v_bodyHeight * 35.0) * neckGuard * 0.12;
    
    // ENHANCED: Bronze composition variation (copper/tin ratio affects color)
    float bronzeVariation = noise(uv * 5.0) * 0.10;
    vec3 richBronze = vec3(0.82, 0.68, 0.42);  // Higher copper
    vec3 paleBronze = vec3(0.75, 0.72, 0.55);  // Higher tin
    
    // ENHANCED: Plume socket (top center mounting point)
    float plumeSocketHeight = smoothstep(0.92, 0.96, v_bodyHeight);
    float plumeSocket = plumeSocketHeight * smoothstep(0.04, 0.02, abs(v_worldPos.x)) * 0.25;
    
    // Bronze sheen using tangent space
    vec3 N = normalize(v_worldNormal);
    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float viewAngle = max(dot(N, V), 0.0);
    float bronzeSheen = pow(viewAngle, 7.0) * 0.25;
    float bronzeFresnel = pow(1.0 - viewAngle, 2.2) * 0.18;

    color = mix(color, richBronze, 0.6);
    color = mix(color, paleBronze, bronzeVariation);
    color += vec3(bronzeSheen + bronzeFresnel + bands + apex);
    color += vec3(cheekGuard + guardEdge + neckGuard + neckSegments + plumeSocket);
    color -= vec3(bronzePatina * 0.4 + verdigris * 0.3);
    color += vec3(hammerMarks * 0.5);
  }
  // LIGHT CHAINMAIL ARMOR (lorica hamata - historically accurate 4-in-1 pattern)
  else if (isArmor) {
    // DRAMATICALLY VISIBLE chainmail - base color much darker than body
    // Start with strong grey base that's clearly NOT skin
    color = color * vec3(0.45, 0.48, 0.52);  // Force grey metal base
    
    // Roman chainmail: butted iron rings, 8-10mm diameter, 1.2mm wire
    vec2 chainUV = v_worldPos.xz * 22.0;  // Larger, more visible rings
    
    // MUCH STRONGER ring pattern - these need to be OBVIOUS
    float rings = chainmailRings(chainUV) * 2.5;  // 3x stronger
    
    // Deep shadows in ring gaps - CRITICAL for visibility
    float ringGaps = (1.0 - chainmailRings(chainUV)) * 0.45;
    
    // Ring structure with STRONG contrast
    float ringHighlight = rings * 0.85;
    
    // ENHANCED: Ring quality variation (handmade vs forge-produced)
    float ringQuality = noise(chainUV * 3.5) * 0.18;  // Size/shape inconsistency
    float wireThickness = noise(chainUV * 28.0) * 0.12;  // Wire gauge variation
    
    // ENHANCED: Micro-gaps between interlocked rings (4-in-1 pattern realism)
    float ringJoints = step(0.88, fract(chainUV.x * 0.5)) * 
                       step(0.88, fract(chainUV.y * 0.5)) * 0.35;
    float gapShadow = ringJoints * smoothstep(0.4, 0.6, rings);
    
    // ENHANCED: Oxidation gradients (rust spreads from contact points)
    float rustSeed = noise(chainUV * 4.2);
    float oxidation = smoothstep(0.45, 0.75, rustSeed) * v_chainmailPhase * 0.32;
    vec3 rustColor = vec3(0.42, 0.32, 0.26);
    vec3 darkRust = vec3(0.28, 0.20, 0.16);
    
    // ENHANCED: Stress points at shoulder/elbow joints
    float jointStress = smoothstep(1.25, 1.35, v_bodyHeight) * // Shoulders
                        smoothstep(1.05, 1.15, v_bodyHeight) *  // Elbows
                        0.25;
    float stressWear = jointStress * noise(chainUV * 8.0) * 0.15;
    
    // Iron oxidation (non-galvanized iron ages naturally)
    float ironTarnish = noise(chainUV * 12.0) * 0.20 * v_chainmailPhase;
    
    // Battle damage - visible broken rings
    float damage = step(0.88, noise(chainUV * 0.8)) * 0.35;
    
    // STRONG specular highlights on ring surfaces
    vec3 N = normalize(v_worldNormal);
    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float viewAngle = max(dot(N, V), 0.0);
    float chainSheen = pow(viewAngle, 4.0) * 0.65;  // Much stronger
    
    // Metallic shimmer as rings catch light
    float shimmer = abs(sin(chainUV.x * 32.0) * sin(chainUV.y * 32.0)) * 0.25;

    // Apply all chainmail effects with STRONG visibility
    color += vec3(ringHighlight + chainSheen + shimmer);
    color -= vec3(ringGaps + damage + gapShadow + stressWear);
    color *= 1.0 + ringQuality - 0.05 + wireThickness;
    color += vec3(ironTarnish * 0.4);
    
    // ENHANCED: Multi-stage oxidation (gradient from bright iron to dark rust)
    color = mix(color, rustColor, oxidation * 0.35);
    color = mix(color, darkRust, oxidation * oxidation * 0.15);  // Deep rust in crevices
    
    // Ensure chainmail is CLEARLY visible - never blend into skin
    color = clamp(color, vec3(0.35), vec3(0.85));
  }
  // LEATHER PTERUGES & BELT (tan/brown leather strips)
  else if (isLegs) {
    // Thick leather with visible grain (using vertex wear data)
    float leatherGrain = noise(uv * 10.0) * 0.16 * (0.5 + v_leatherWear * 0.5);
    float leatherPores = noise(uv * 22.0) * 0.08;

    // Pteruges strip pattern
    float strips = pterugesStrips(v_worldPos.xz, v_bodyHeight);

    // Worn leather edges
    float wear = noise(uv * 4.0) * v_leatherWear * 0.10 - 0.05;

    // Leather has subtle sheen
    vec3 N = normalize(v_worldNormal);
    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float viewAngle = max(dot(N, V), 0.0);
    float leatherSheen = pow(1.0 - viewAngle, 4.5) * 0.10;

    color *= 1.0 + leatherGrain + leatherPores - 0.08 + wear;
    color += vec3(strips * 0.15 + leatherSheen);
  }
  // SCUTUM SHIELD (curved laminated wood with metal boss)
  else if (isShield) {
    // Shield boss (metal center dome for deflection)
    float bossDist = length(v_worldPos.xy);
    float boss = smoothstep(0.12, 0.08, bossDist) * 0.6;
    float bossRim = smoothstep(0.14, 0.12, bossDist) * 
                    smoothstep(0.10, 0.12, bossDist) * 0.3;
    
    // Laminated wood layers (3-ply construction visible at edges)
    float edgeDist = max(abs(v_worldPos.x), abs(v_worldPos.y));
    float edgeWear = smoothstep(0.42, 0.48, edgeDist);
    float woodLayers = edgeWear * noise(uv * 40.0) * 0.25;
    
    // Leather/linen facing (painted surface)
    float fabricGrain = noise(uv * 25.0) * 0.08;
    float canvasWeave = sin(uv.x * 60.0) * sin(uv.y * 58.0) * 0.04;
    
    // Metal edging (bronze trim around perimeter)
    float metalEdge = smoothstep(0.46, 0.48, edgeDist) * 0.45;
    vec3 bronzeEdge = vec3(0.75, 0.55, 0.30);
    
    // Battle damage (dents, cuts, scratches)
    float dents = noise(uv * 8.0) * 0.15;
    float cuts = step(0.92, noise(uv * 12.0)) * 0.25;
    
    // Boss metallic sheen (polished bronze)
    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);
    float bossSheen = pow(viewAngle, 10.0) * boss * 0.55;
    
    color *= 1.0 + fabricGrain + canvasWeave - 0.05;
    color += vec3(boss * 0.4 + bossRim * 0.25 + bossSheen);
    color += vec3(woodLayers * 0.3);
    color = mix(color, bronzeEdge, metalEdge);
    color -= vec3(dents * 0.08 + cuts * 0.15);
  }
  // BOW & ARROWS (composite wood, sinew, horn)
  else if (isWeapon) {
    // Composite bow construction (wood core, horn belly, sinew back)
    bool isBowLimb = (v_bodyHeight > 0.3 && v_bodyHeight < 0.9);
    bool isGrip = (v_bodyHeight >= 0.4 && v_bodyHeight <= 0.6);
    bool isString = (v_bodyHeight < 0.05 || v_bodyHeight > 0.95);
    
    if (isBowLimb) {
      // Wood grain with horn lamination
      float woodGrain = noise(uv * 35.0) * 0.18;
      float hornBanding = abs(sin(v_bodyHeight * 25.0)) * 0.12;
      
      vec3 V = normalize(vec3(0.0, 1.0, 0.4));
      float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);
      float hornSheen = pow(1.0 - viewAngle, 6.0) * 0.15;
      
      color *= 1.0 + woodGrain + hornBanding;
      color += vec3(hornSheen);
    } else if (isGrip) {
      // Leather wrap with cord binding
      float leatherWrap = noise(uv * 20.0) * 0.14;
      float cordBinding = step(0.92, fract(v_worldPos.y * 30.0)) * 0.15;
      color *= vec3(0.55, 0.42, 0.32); // Dark leather
      color *= 1.0 + leatherWrap;
      color += vec3(cordBinding);
    } else if (isString) {
      // Sinew bowstring (natural fiber)
      color = vec3(0.72, 0.68, 0.60); // Natural sinew color
      float fiberTwist = noise(uv * 80.0) * 0.10;
      color *= 1.0 + fiberTwist;
    }
  }

  color = clamp(color, 0.0, 1.0);
  
  // === PHASE 4: ADVANCED FEATURES ===
  // Environmental interactions and battle wear (procedural, no new uniforms needed)
  
  // Battle-hardened appearance (more wear on lower body parts)
  float campaignWear = (1.0 - v_bodyHeight * 0.6) * 0.15;
  float dustAccumulation = noise(v_worldPos.xz * 8.0) * campaignWear * 0.12;
  
  // Rain streaks (vertical weathering patterns)
  float rainStreaks = smoothstep(0.85, 0.92, noise(v_worldPos.xz * 2.5 + vec2(0.0, v_worldPos.y * 8.0))) * 
                      v_bodyHeight * 0.08;  // More visible on upper body
  
  // Mud splatter (lower body, procedural based on position)
  float mudHeight = smoothstep(0.5, 0.2, v_bodyHeight);  // Concentrated at feet/legs
  float mudPattern = step(0.75, noise(v_worldPos.xz * 12.0 + v_worldPos.y * 3.0));
  float mudSplatter = mudHeight * mudPattern * 0.18;
  vec3 mudColor = vec3(0.22, 0.18, 0.14);
  
  // Apply environmental effects
  color -= vec3(dustAccumulation);  // Dust darkens surfaces
  color -= vec3(rainStreaks * 0.5);  // Rain creates dark streaks
  color = mix(color, mudColor, mudSplatter);  // Mud obscures material

  // Lighting model per material
  vec3 lightDir = normalize(vec3(1.0, 1.15, 1.0));
  float nDotL = dot(normal, lightDir);

  float wrapAmount = isHelmet ? 0.15 : (isArmor ? 0.22 : 0.38);
  float diff = max(nDotL * (1.0 - wrapAmount) + wrapAmount, 0.22);

  if (isHelmet) {
    diff = pow(diff, 0.90);
  }

  color *= diff;
  FragColor = vec4(color, u_alpha);
}
