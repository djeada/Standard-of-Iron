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
in float v_platePhase;
in float v_segmentStress;
in float v_rivetPattern;
in float v_polishLevel;

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
// MATERIAL PATTERN FUNCTIONS (Roman Swordsman - Legionary)
// ============================================================================

// Segmented armor plates (lorica segmentata)
float armorPlates(vec2 p, float y) {
  float plateY = fract(y * 6.5);
  float plateLine = smoothstep(0.90, 0.98, plateY) * 0.12;
  float rivetX = fract(p.x * 18.0);
  float rivet = smoothstep(0.48, 0.50, rivetX) * smoothstep(0.52, 0.50, rivetX);
  float rivetPattern = rivet * step(0.92, plateY) * 0.25;
  return plateLine + rivetPattern;
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
  vec2 uv = v_worldPos.xz * 5.0;

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

  // === ROMAN SWORDSMAN (LEGIONARY) MATERIALS ===

  // HEAVY STEEL HELMET (galea - cool blue-grey steel)
  if (isHelmet) {
    // Polished steel finish with vertex polish level
    float brushedMetal = abs(sin(v_worldPos.y * 95.0)) * 0.02;
    float scratches = noise(uv * 35.0) * 0.018 * (1.0 - v_polishLevel * 0.5);
    float dents = noise(uv * 8.0) * 0.025;

    // Use vertex-computed helmet detail
    float bands = v_helmetDetail * 0.12;
    float rivets = v_rivetPattern * 0.12;

    // ENHANCED: Elaborate cheek guards (legionary standard)
    float cheekGuardHeight = smoothstep(0.68, 0.78, v_bodyHeight) *
                             smoothstep(0.92, 0.82, v_bodyHeight);
    float cheekX = abs(v_worldPos.x);
    float cheekGuard = cheekGuardHeight * smoothstep(0.14, 0.10, cheekX) * 0.45;
    float guardEdge =
        cheekGuard * smoothstep(0.36, 0.40, noise(uv * 16.0)) * 0.22;
    float hingeDetail =
        cheekGuard * step(0.92, fract(v_bodyHeight * 32.0)) * 0.25;

    // ENHANCED: Extended neck guard (deep rear protection)
    float neckGuardHeight = smoothstep(0.64, 0.70, v_bodyHeight) *
                            smoothstep(0.84, 0.74, v_bodyHeight);
    float behindHead = step(v_worldNormal.z, -0.20);
    float neckGuard = neckGuardHeight * behindHead * 0.38;
    float neckLamellae =
        step(0.82, fract(v_bodyHeight * 48.0)) * neckGuard * 0.18;
    float neckRivets =
        step(0.90, fract(v_worldPos.x * 22.0)) * neckGuard * 0.12;

    // ENHANCED: Reinforced brow band (legionary frontal ridge)
    float browHeight = smoothstep(0.82, 0.86, v_bodyHeight) *
                       smoothstep(0.94, 0.88, v_bodyHeight);
    float frontFacing = step(0.4, v_worldNormal.z);
    float browReinforce = browHeight * frontFacing * 0.28;
    float browDecoration = browReinforce * noise(uv * 20.0) * 0.10;

    // ENHANCED: Plume socket with mounting bracket (officer insignia)
    float plumeSocketHeight = smoothstep(0.88, 0.92, v_bodyHeight);
    float plumeSocket =
        plumeSocketHeight * smoothstep(0.06, 0.04, abs(v_worldPos.x)) * 0.32;
    float socketBracket = plumeSocket * step(0.85, noise(uv * 15.0)) * 0.15;

    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);
    float fresnel = pow(1.0 - viewAngle, 1.8) * 0.35;
    float specular = pow(viewAngle, 12.0) * 0.55 * v_polishLevel;
    float skyReflection = (v_worldNormal.y * 0.5 + 0.5) * 0.12;

    color += vec3(fresnel + skyReflection + specular * 1.8 + bands + rivets);
    color += vec3(cheekGuard + guardEdge + hingeDetail);
    color += vec3(neckGuard + neckLamellae + neckRivets);
    color += vec3(browReinforce + browDecoration);
    color += vec3(plumeSocket + socketBracket);
    color += vec3(brushedMetal);
    color -= vec3(scratches + dents * 0.4);
  }
  // HEAVY SEGMENTED ARMOR (lorica segmentata - iconic Roman plate armor)
  else if (isArmor) {
    // FORCE polished steel base - segmentata is BRIGHT, REFLECTIVE armor
    color = vec3(0.72, 0.78, 0.88); // Bright steel base - NOT skin tone!

    vec2 armorUV = v_worldPos.xz * 5.5;

    // === HORIZONTAL PLATE BANDS - MUST BE OBVIOUS ===
    // 6-7 clearly visible bands wrapping torso
    float bandPattern = fract(v_platePhase);

    // STRONG band edges (plate separations)
    float bandEdge = step(0.92, bandPattern) + step(bandPattern, 0.08);
    float plateLine = bandEdge * 0.55; // Much stronger

    // DEEP shadows between overlapping plates
    float overlapShadow = smoothstep(0.90, 0.98, bandPattern) * 0.65;

    // Alternating plate brightness (polishing variation)
    float plateBrightness = step(0.5, fract(v_platePhase * 0.5)) * 0.15;

    // === RIVETS - CLEARLY VISIBLE ===
    // Large brass rivets along each band edge
    float rivetX = fract(v_worldPos.x * 16.0);
    float rivetY = fract(v_platePhase * 6.5); // Align with bands
    float rivet = smoothstep(0.45, 0.50, rivetX) *
                  smoothstep(0.55, 0.50, rivetX) *
                  (step(0.92, rivetY) + step(rivetY, 0.08));
    float brassRivets = rivet * 0.45;         // Much more visible
    vec3 brassColor = vec3(0.95, 0.82, 0.45); // Bright brass

    // === METALLIC FINISH ===
    // Polished steel with strong reflections
    float brushedMetal = abs(sin(v_worldPos.y * 75.0)) * 0.12;

    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);

    // VERY STRONG specular - legionary armor was highly polished
    float plateSpecular = pow(viewAngle, 9.0) * 0.85 * v_polishLevel;

    // Metallic fresnel
    float plateFresnel = pow(1.0 - viewAngle, 2.2) * 0.45;

    // Sky reflection
    float skyReflect = (v_worldNormal.y * 0.5 + 0.5) * 0.35 * v_polishLevel;

    // === WEAR & DAMAGE ===
    // Battle scratches
    float scratches =
        noise(armorUV * 42.0) * 0.08 * (1.0 - v_polishLevel * 0.7);

    // Impact dents (front armor takes hits)
    float frontFacing = smoothstep(-0.2, 0.7, v_worldNormal.z);
    float dents = noise(armorUV * 6.0) * 0.12 * frontFacing;

    // ENHANCED: Joint wear between plates with articulation stress
    float jointWear = v_segmentStress * 0.25;

    // ENHANCED: Plate edge wear (horizontal band edges show most damage)
    float edgeWear = smoothstep(0.88, 0.96, bandPattern) *
                     smoothstep(0.12, 0.04, bandPattern) * 0.18;
    float edgeDarkening = edgeWear * noise(armorUV * 15.0) * 0.22;

    // ENHANCED: Rivet stress patterns (rivets loosen/oxidize at stress points)
    float rivetStress = rivet * v_segmentStress * 0.15;
    vec3 rivetOxidation = vec3(0.35, 0.28, 0.20) * rivetStress;

    // ENHANCED: Plate segment definition (each band slightly different
    // polish/color)
    float segmentVariation = noise(vec2(v_platePhase, 0.5) * 2.0) * 0.08;

    // ENHANCED: Overlapping plate shadows with depth (more realistic
    // articulation)
    float plateDepth = smoothstep(0.94, 0.98, bandPattern) * 0.40;
    float gapShadow = plateDepth * (1.0 - viewAngle * 0.5); // Deeper in gaps

    // Apply all plate effects - STRONG VISIBILITY
    color += vec3(plateBrightness + plateSpecular + plateFresnel + skyReflect +
                  brushedMetal + segmentVariation);
    color -= vec3(plateLine * 0.4 + overlapShadow + scratches + dents * 0.5 +
                  jointWear + edgeDarkening + gapShadow);

    // Add brass rivets with color and oxidation
    color = mix(color, brassColor, brassRivets);
    color -= rivetOxidation;

    // Ensure segmentata is ALWAYS bright and visible
    color = clamp(color, vec3(0.45), vec3(0.95));
  }
  // LEATHER PTERUGES & BELT
  else if (isLegs) {
    float leatherGrain = noise(uv * 10.0) * 0.15;
    float strips = pterugesStrips(v_worldPos.xz, v_bodyHeight);
    float wearMarks = noise(uv * 3.0) * 0.10;

    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);
    float leatherSheen = pow(1.0 - viewAngle, 4.5) * 0.10;

    color *= 1.0 + leatherGrain - 0.08 + wearMarks - 0.05;
    color += vec3(strips * 0.15 + leatherSheen);
  }
  // SCUTUM SHIELD (curved laminated wood with metal boss)
  else if (isShield) {
    // Shield boss (raised metal dome)
    float bossDist = length(v_worldPos.xy);
    float boss = smoothstep(0.12, 0.08, bossDist) * 0.6;
    float bossRim = smoothstep(0.14, 0.12, bossDist) *
                    smoothstep(0.10, 0.12, bossDist) * 0.3;

    // Laminated wood construction
    float edgeDist = max(abs(v_worldPos.x), abs(v_worldPos.y));
    float edgeWear = smoothstep(0.42, 0.48, edgeDist);
    float woodLayers = edgeWear * noise(uv * 40.0) * 0.25;

    // Painted canvas facing
    float fabricGrain = noise(uv * 25.0) * 0.08;
    float canvasWeave = sin(uv.x * 60.0) * sin(uv.y * 58.0) * 0.04;

    // Bronze edging (perimeter reinforcement)
    float metalEdge = smoothstep(0.46, 0.48, edgeDist) * 0.45;
    vec3 bronzeEdge = vec3(0.75, 0.55, 0.30);

    // Combat damage (more on legionary shields)
    float dents = noise(uv * 8.0) * v_polishLevel * 0.20;
    float cuts = step(0.88, noise(uv * 12.0)) * 0.32;
    float scratches = noise(uv * 50.0) * 0.15;

    // Boss polish and reflection
    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);
    float bossSheen = pow(viewAngle, 12.0) * boss * v_polishLevel * 0.60;

    color *= 1.0 + fabricGrain + canvasWeave - 0.05;
    color += vec3(boss * 0.4 + bossRim * 0.25 + bossSheen);
    color += vec3(woodLayers * 0.3);
    color = mix(color, bronzeEdge, metalEdge);
    color -= vec3(dents * 0.08 + cuts * 0.12 + scratches * 0.05);
  }
  // GLADIUS (Roman short sword - steel blade)
  else if (isWeapon) {
    // Weapon parts by body height
    bool isBlade = (v_bodyHeight > 0.35); // Blade section
    bool isGrip = (v_bodyHeight > 0.15 && v_bodyHeight <= 0.35); // Handle
    bool isPommel = (v_bodyHeight <= 0.15);                      // Pommel/guard

    if (isBlade) {
      // Polished steel blade
      vec3 steelColor = vec3(0.78, 0.81, 0.85);

      // Fuller groove (weight reduction channel)
      float fullerX = abs(v_worldPos.x);
      float fuller = smoothstep(0.025, 0.015, fullerX) * 0.25;

      // Razor-sharp edge
      float edge = smoothstep(0.08, 0.10, fullerX) * 0.50;

      // High polish finish (legionary standard)
      float polishMarks = noise(uv * 60.0) * 0.08 * v_polishLevel;

      // Edge tempering line (heat treatment)
      float temperLine = smoothstep(0.075, 0.085, fullerX) * 0.12;

      vec3 V = normalize(vec3(0.0, 1.0, 0.5));
      float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);
      float metalSheen = pow(viewAngle, 18.0) * v_polishLevel * 0.70;
      float reflection = pow(viewAngle, 8.0) * 0.35;

      color = mix(color, steelColor, 0.8);
      color -= vec3(fuller);
      color += vec3(edge + metalSheen + reflection);
      color += vec3(polishMarks * 0.6 + temperLine * 0.4);
    } else if (isGrip) {
      // Wood grip with leather wrap
      float woodBase = noise(uv * 28.0) * 0.14;
      float leatherWrap = noise(uv * 35.0) * 0.12;
      float wireBinding = step(0.88, fract(v_worldPos.y * 30.0)) * 0.20;

      color *= vec3(0.52, 0.42, 0.32); // Leather-wrapped wood
      color *= 1.0 + woodBase + leatherWrap;
      color += vec3(wireBinding * 0.5); // Bronze wire
    } else if (isPommel) {
      // Brass pommel (counterweight and decoration)
      vec3 brassColor = vec3(0.82, 0.72, 0.42);

      float decoration = noise(uv * 20.0) * 0.10;

      vec3 V = normalize(vec3(0.0, 1.0, 0.5));
      float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);
      float brassSheen = pow(viewAngle, 12.0) * 0.50;

      color = mix(color, brassColor, 0.8);
      color += vec3(brassSheen + decoration * 0.4);
    }
  }

  color = clamp(color, 0.0, 1.0);

  // === PHASE 4: ADVANCED FEATURES ===
  // Environmental interactions (elite legionary maintains higher standards)

  // Limited campaign wear (legionaries keep equipment polished)
  float campaignWear =
      (1.0 - v_bodyHeight * 0.7) * (1.0 - v_polishLevel) * 0.12;
  float dustAccumulation = noise(v_worldPos.xz * 8.0) * campaignWear * 0.08;

  // Rain streaks (even polished armor shows weather)
  float rainStreaks =
      smoothstep(0.87, 0.93,
                 noise(v_worldPos.xz * 2.5 + vec2(0.0, v_worldPos.y * 8.0))) *
      v_bodyHeight * 0.08; // Subtle on segmentata

  // Minimal mud (legionaries in formation, not skirmishing)
  float mudHeight = smoothstep(0.45, 0.10, v_bodyHeight); // Only ankles/feet
  float mudPattern =
      step(0.80, noise(v_worldPos.xz * 12.0 + v_worldPos.y * 3.0));
  float mudSplatter = mudHeight * mudPattern * 0.15;
  vec3 mudColor = vec3(0.22, 0.18, 0.14);

  // Blood evidence (frontline combat)
  float bloodHeight = smoothstep(0.88, 0.65, v_bodyHeight); // Upper torso/arms
  float bloodPattern =
      step(0.90, noise(v_worldPos.xz * 15.0 + v_bodyHeight * 5.0));
  float bloodStain =
      bloodHeight * bloodPattern * (1.0 - v_polishLevel * 0.5) * 0.10;
  vec3 bloodColor = vec3(0.18, 0.08, 0.06);

  // Apply environmental effects (moderated by polish level)
  color -= vec3(dustAccumulation);
  color -= vec3(rainStreaks * 0.5);
  color = mix(color, mudColor, mudSplatter);
  color = mix(color, bloodColor, bloodStain);

  // Lighting per material
  vec3 lightDir = normalize(vec3(1.0, 1.2, 1.0));
  float nDotL = dot(normalize(v_worldNormal), lightDir);

  float wrapAmount = isHelmet ? 0.08 : (isArmor ? 0.10 : 0.30);
  float diff = max(nDotL * (1.0 - wrapAmount) + wrapAmount, 0.18);

  // Extra contrast for polished steel
  if (isHelmet || isArmor) {
    diff = pow(diff, 0.85);
  }

  color *= diff;
  FragColor = vec4(color, u_alpha);
}
