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
in float v_steelWear;
in float v_chainmailPhase;
in float v_rivetPattern;
in float v_leatherWear;

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
// MATERIAL PATTERN FUNCTIONS (Roman Spearman - Hastatus)
// ============================================================================

float chainmailRings(vec2 p) {
  vec2 grid = fract(p * 32.0) - 0.5;
  float ring = length(grid);
  float ringPattern =
      smoothstep(0.38, 0.32, ring) - smoothstep(0.28, 0.22, ring);
  vec2 offsetGrid = fract(p * 32.0 + vec2(0.5, 0.0)) - 0.5;
  float offsetRing = length(offsetGrid);
  float offsetPattern =
      smoothstep(0.38, 0.32, offsetRing) - smoothstep(0.28, 0.22, offsetRing);
  return (ringPattern + offsetPattern) * 0.14;
}

float pterugesStrips(vec2 p, float y) {
  float stripX = fract(p.x * 9.0);
  float strip = smoothstep(0.15, 0.20, stripX) - smoothstep(0.80, 0.85, stripX);
  float leatherTex = noise(p * 18.0) * 0.35;
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
    isArmor = false; // Body mesh should not get armor effects
  }

  bool isLegs = (v_armorLayer >= 1.5);

  // === ROMAN SPEARMAN (HASTATUS) MATERIALS ===

  // HEAVY STEEL HELMET (cool blue-grey steel)
  if (isHelmet) {
    // Steel wear patterns from vertex shader
    float brushed = abs(sin(v_worldPos.y * 95.0)) * 0.020;
    float dents = noise(uv * 6.5) * 0.032 * v_steelWear;
    float rustTex = noise(uv * 9.0) * 0.11 * v_steelWear;

    // Use vertex-computed helmet detail (reinforcement bands, brow band, cheek
    // guards)
    float bands = v_helmetDetail * 0.12;
    float rivets = v_rivetPattern * 0.10;

    // ENHANCED: Cheek guards (hinged steel plates with hinge pins)
    float cheekGuardHeight = smoothstep(0.70, 0.80, v_bodyHeight) *
                             smoothstep(0.90, 0.80, v_bodyHeight);
    float cheekX = abs(v_worldPos.x);
    float cheekGuard = cheekGuardHeight * smoothstep(0.12, 0.09, cheekX) * 0.40;
    float hingePins =
        cheekGuard * step(0.94, fract(v_bodyHeight * 28.0)) * 0.20;
    float guardRepousse =
        cheekGuard * noise(uv * 12.0) * 0.08; // Decorative embossing

    // ENHANCED: Neck guard (segmented rear protection)
    float neckGuardHeight = smoothstep(0.66, 0.72, v_bodyHeight) *
                            smoothstep(0.82, 0.72, v_bodyHeight);
    float behindHead = step(v_worldNormal.z, -0.25);
    float neckGuard = neckGuardHeight * behindHead * 0.32;
    float neckSegments =
        step(0.85, fract(v_bodyHeight * 42.0)) * neckGuard * 0.15;

    // ENHANCED: Brow reinforcement (frontal impact protection)
    float browHeight = smoothstep(0.84, 0.88, v_bodyHeight) *
                       smoothstep(0.92, 0.88, v_bodyHeight);
    float frontFacing = step(0.3, v_worldNormal.z);
    float browReinforce = browHeight * frontFacing * 0.22;

    // ENHANCED: Plume socket (central crest mount)
    float plumeSocketHeight = smoothstep(0.90, 0.94, v_bodyHeight);
    float plumeSocket =
        plumeSocketHeight * smoothstep(0.05, 0.03, abs(v_worldPos.x)) * 0.28;

    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);
    float steelSheen = pow(viewAngle, 9.0) * 0.30;
    float steelFresnel = pow(1.0 - viewAngle, 2.0) * 0.22;

    color += vec3(steelSheen + steelFresnel + bands + rivets);
    color += vec3(cheekGuard + hingePins + guardRepousse);
    color += vec3(neckGuard + neckSegments + browReinforce + plumeSocket);
    color -= vec3(rustTex * 0.3);
    color += vec3(brushed * 0.6);
  }
  // LIGHT CHAINMAIL ARMOR (lorica hamata - pectorale reinforcement optional)
  else if (isArmor) {
    // FORCE grey metal base - chainmail is CLEARLY not skin
    color = color * vec3(0.42, 0.46, 0.50); // Darker grey base

    vec2 chainUV = v_worldPos.xz * 22.0; // Larger rings

    // PECTORALE CHEST PLATE - highly visible steel plate overlay
    float chestPlate =
        smoothstep(1.15, 1.25, v_bodyHeight) *
        smoothstep(1.55, 1.45, v_bodyHeight) *
        smoothstep(0.25, 0.15, abs(v_worldPos.x)); // Center chest

    // STRONG distinction between plate and chainmail
    if (chestPlate > 0.3) {
      // PECTORALE - polished steel plate
      color = vec3(0.72, 0.76, 0.82); // Bright steel

      // Plate edges and rivets
      float plateEdge = smoothstep(0.88, 0.92, chestPlate) * 0.25;
      float rivets = step(0.92, fract(v_worldPos.x * 18.0)) *
                     step(0.92, fract(v_worldPos.y * 12.0)) * 0.30;

      vec3 V = normalize(vec3(0.0, 1.0, 0.5));
      float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);
      float plateSheen = pow(viewAngle, 8.0) * 0.75; // Strong reflection

      color += vec3(plateSheen + rivets);
      color -= vec3(plateEdge);
    } else {
      // CHAINMAIL - much more visible rings with enhanced realism
      float rings = chainmailRings(chainUV) * 2.2;
      float ringGaps = (1.0 - chainmailRings(chainUV)) * 0.50;

      // ENHANCED: Ring quality variation and micro-gaps
      float ringQuality = noise(chainUV * 3.5) * 0.18;
      float wireThickness = noise(chainUV * 28.0) * 0.12;
      float ringJoints = step(0.88, fract(chainUV.x * 0.5)) *
                         step(0.88, fract(chainUV.y * 0.5)) * 0.35;
      float gapShadow = ringJoints * smoothstep(0.4, 0.6, rings);

      // ENHANCED: Oxidation gradients spreading from contact points
      float rustSeed = noise(chainUV * 4.2);
      float oxidation = smoothstep(0.45, 0.75, rustSeed) * v_steelWear * 0.28;
      vec3 rustColor = vec3(0.38, 0.28, 0.22);
      vec3 darkRust = vec3(0.28, 0.20, 0.16);

      // ENHANCED: Stress points at armor articulation zones
      float jointStress = smoothstep(1.20, 1.30, v_bodyHeight) * // Shoulders
                          smoothstep(1.00, 1.10, v_bodyHeight) * // Elbows
                          0.25;
      float stressWear = jointStress * noise(chainUV * 8.0) * 0.15;

      // Battle damage
      float damage = step(0.86, noise(chainUV * 0.8)) * 0.40;

      vec3 V = normalize(vec3(0.0, 1.0, 0.5));
      float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);
      float chainSheen = pow(viewAngle, 4.5) * 0.55;

      float shimmer = abs(sin(chainUV.x * 32.0) * sin(chainUV.y * 32.0)) * 0.22;

      color += vec3(rings + chainSheen + shimmer);
      color -= vec3(ringGaps + damage + gapShadow + stressWear);
      color *= 1.0 + ringQuality - 0.05 + wireThickness;

      // Multi-stage oxidation with rust progression
      color = mix(color, rustColor, oxidation * 0.40);
      color = mix(color, darkRust, oxidation * oxidation * 0.15);
    }

    // Ensure armor is ALWAYS clearly visible
    color = clamp(color, vec3(0.32), vec3(0.88));
  }
  // LEATHER PTERUGES & BELT
  else if (isLegs) {
    float leatherGrain = noise(uv * 10.0) * 0.16 * (0.5 + v_leatherWear * 0.5);
    float leatherPores = noise(uv * 22.0) * 0.08;
    float strips = pterugesStrips(v_worldPos.xz, v_bodyHeight);
    float wear = noise(uv * 4.0) * v_leatherWear * 0.10 - 0.05;

    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);
    float leatherSheen = pow(1.0 - viewAngle, 4.5) * 0.10;

    color *= 1.0 + leatherGrain + leatherPores - 0.08 + wear;
    color += vec3(strips * 0.15 + leatherSheen);
  }
  // SCUTUM SHIELD (curved laminated wood with metal boss)
  else if (isShield) {
    // Shield boss (domed metal center)
    float bossDist = length(v_worldPos.xy);
    float boss = smoothstep(0.12, 0.08, bossDist) * 0.6;
    float bossRim = smoothstep(0.14, 0.12, bossDist) *
                    smoothstep(0.10, 0.12, bossDist) * 0.3;

    // Wood construction layers
    float edgeDist = max(abs(v_worldPos.x), abs(v_worldPos.y));
    float edgeWear = smoothstep(0.42, 0.48, edgeDist);
    float woodLayers = edgeWear * noise(uv * 40.0) * 0.25;

    // Canvas facing
    float fabricGrain = noise(uv * 25.0) * 0.08;
    float canvasWeave = sin(uv.x * 60.0) * sin(uv.y * 58.0) * 0.04;

    // Bronze edging
    float metalEdge = smoothstep(0.46, 0.48, edgeDist) * 0.45;
    vec3 bronzeEdge = vec3(0.75, 0.55, 0.30);

    // Battle wear
    float dents = noise(uv * 8.0) * v_steelWear * 0.18;
    float cuts = step(0.90, noise(uv * 12.0)) * 0.28;

    // Boss reflection
    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);
    float bossSheen = pow(viewAngle, 10.0) * boss * 0.55;

    color *= 1.0 + fabricGrain + canvasWeave - 0.05;
    color += vec3(boss * 0.4 + bossRim * 0.25 + bossSheen);
    color += vec3(woodLayers * 0.3);
    color = mix(color, bronzeEdge, metalEdge);
    color -= vec3(dents * 0.08 + cuts * 0.12);
  }
  // PILUM & GLADIUS (iron spear and steel sword)
  else if (isWeapon) {
    // Determine weapon part by height
    bool isBlade = (v_bodyHeight > 0.5); // Upper half is blade/spearhead
    bool isShaft = (v_bodyHeight > 0.2 && v_bodyHeight <= 0.5); // Mid section
    bool isGrip = (v_bodyHeight <= 0.2);                        // Lower section

    if (isBlade) {
      // Iron spearhead or steel blade
      vec3 steelColor = vec3(0.72, 0.75, 0.80);

      // Fuller groove (blood channel)
      float fullerX = abs(v_worldPos.x);
      float fuller = smoothstep(0.03, 0.01, fullerX) * 0.20;

      // Edge sharpness (bright along cutting edge)
      float edge = smoothstep(0.09, 0.10, fullerX) * 0.45;

      // Forging marks and tempering
      float forgeMarks = noise(uv * 45.0) * 0.10;
      float tempering = abs(sin(v_bodyHeight * 35.0)) * 0.08;

      vec3 V = normalize(vec3(0.0, 1.0, 0.5));
      float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);
      float metalSheen = pow(viewAngle, 15.0) * 0.60;

      color = mix(color, steelColor, 0.7);
      color -= vec3(fuller);
      color += vec3(edge + metalSheen);
      color += vec3(forgeMarks * 0.5 + tempering * 0.3);
    } else if (isShaft) {
      // Ash wood shaft
      float woodGrain = noise(uv * 32.0) * 0.16;
      float rings = abs(sin(v_worldPos.y * 40.0)) * 0.08;
      color *= vec3(0.68, 0.58, 0.45); // Ash wood color
      color *= 1.0 + woodGrain + rings;
    } else if (isGrip) {
      // Leather-wrapped grip
      float leatherWrap = noise(uv * 22.0) * 0.14;
      float binding = step(0.90, fract(v_worldPos.y * 25.0)) * 0.18;
      color *= vec3(0.48, 0.38, 0.28); // Dark leather
      color *= 1.0 + leatherWrap;
      color += vec3(binding);
    }
  }

  color = clamp(color, 0.0, 1.0);

  // === PHASE 4: ADVANCED FEATURES ===
  // Environmental interactions and enhanced battle wear

  // Campaign wear (medium infantry shows moderate weathering)
  float campaignWear = (1.0 - v_bodyHeight * 0.5) * 0.18;
  float dustAccumulation = noise(v_worldPos.xz * 8.0) * campaignWear * 0.14;

  // Rain streaks (vertical weathering on steel armor)
  float rainStreaks =
      smoothstep(0.85, 0.92,
                 noise(v_worldPos.xz * 2.5 + vec2(0.0, v_worldPos.y * 8.0))) *
      v_bodyHeight * 0.10; // Visible on pectorale

  // Mud splatter (frontline combat conditions)
  float mudHeight = smoothstep(0.55, 0.15, v_bodyHeight);
  float mudPattern =
      step(0.72, noise(v_worldPos.xz * 12.0 + v_worldPos.y * 3.0));
  float mudSplatter = mudHeight * mudPattern * 0.22;
  vec3 mudColor = vec3(0.22, 0.18, 0.14);

  // Blood stains (combat evidence on armor/shield)
  float bloodHeight = smoothstep(0.85, 0.60, v_bodyHeight); // Torso/arms
  float bloodPattern =
      step(0.88, noise(v_worldPos.xz * 15.0 + v_bodyHeight * 5.0));
  float bloodStain = bloodHeight * bloodPattern * v_steelWear * 0.12;
  vec3 bloodColor = vec3(0.18, 0.08, 0.06); // Dried blood (dark brown)

  // Apply environmental effects
  color -= vec3(dustAccumulation);
  color -= vec3(rainStreaks * 0.6);
  color = mix(color, mudColor, mudSplatter);
  color = mix(color, bloodColor, bloodStain);

  // Lighting per material
  vec3 lightDir = normalize(vec3(1.0, 1.15, 1.0));
  float nDotL = dot(normalize(v_worldNormal), lightDir);

  float wrapAmount = isHelmet ? 0.12 : (isArmor ? 0.22 : 0.35);
  float diff = max(nDotL * (1.0 - wrapAmount) + wrapAmount, 0.20);

  if (isHelmet) {
    diff = pow(diff, 0.88);
  }

  color *= diff;
  FragColor = vec4(color, u_alpha);
}
