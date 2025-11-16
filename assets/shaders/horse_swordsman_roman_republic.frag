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
in float v_armorLayer; // Armor layer from vertex shader
in float v_bodyHeight;
in float v_armorSheen;
in float v_leatherWear;
in float v_horseMusculature;
in float v_hairFlow;
in float v_hoofWear;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_useTexture;
uniform float u_alpha;
uniform int u_materialId;

out vec4 FragColor;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================
const float PI = 3.14159265359;

float saturate(float x) { return clamp(x, 0.0, 1.0); }
vec3 saturate(vec3 x) { return clamp(x, 0.0, 1.0); }

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
  float a = 0.5;
  float f = 0.0;
  for (int i = 0; i < 5; ++i) {
    f += a * noise(p);
    p *= 2.03;
    a *= 0.5;
  }
  return f;
}

// anti-aliased step
float aaStep(float edge, float x) {
  float w = fwidth(x);
  return smoothstep(edge - w, edge + w, x);
}

// ============================================================================
// MATERIAL PATTERN FUNCTIONS (Horse & Rider)
// ============================================================================

// ---------------------
// patterns
// ---------------------

// plate seams + rivets (AA)
float armorPlates(vec2 p, float y) {
  float plateY = fract(y * 6.5);
  float line = smoothstep(0.92, 0.98, plateY) - smoothstep(0.98, 1.0, plateY);
  // anti-aliased line thickness
  line = smoothstep(0.0, fwidth(plateY) * 2.0, line) * 0.12;

  // rivets on top seams
  float rivetX = fract(p.x * 18.0);
  float rivet = smoothstep(0.48, 0.50, rivetX) * smoothstep(0.52, 0.50, rivetX);
  rivet *= step(0.92, plateY);
  return line + rivet * 0.25;
}

// linked ring suggestion (AA)
float chainmailRings(vec2 p) {
  vec2 uv = p * 35.0;

  vec2 g0 = fract(uv) - 0.5;
  float r0 = length(g0);
  float fw0 = fwidth(r0) * 1.2;
  float ring0 = smoothstep(0.30 + fw0, 0.30 - fw0, r0) -
                smoothstep(0.20 + fw0, 0.20 - fw0, r0);

  vec2 g1 = fract(uv + vec2(0.5, 0.0)) - 0.5;
  float r1 = length(g1);
  float fw1 = fwidth(r1) * 1.2;
  float ring1 = smoothstep(0.30 + fw1, 0.30 - fw1, r1) -
                smoothstep(0.20 + fw1, 0.20 - fw1, r1);

  return (ring0 + ring1) * 0.15;
}

float horseHidePattern(vec2 p) {
  float grain = fbm(p * 80.0) * 0.10;
  float ripple = sin(p.x * 22.0) * cos(p.y * 28.0) * 0.035;
  float mottling = smoothstep(0.55, 0.65, fbm(p * 6.0)) * 0.07;
  return grain + ripple + mottling;
}

// ============================================================================
// PBR FUNCTIONS (Microfacet BRDF)
// ============================================================================

// ---------------------
// microfacet shading
// ---------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
  return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float D_GGX(float NdotH, float rough) {
  float a = max(0.001, rough);
  float a2 = a * a;
  float d = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
  return a2 / max(1e-6, (PI * d * d));
}

float G_Smith(float NdotV, float NdotL, float rough) {
  float r = rough + 1.0;
  float k = (r * r) / 8.0;
  float gV = NdotV / (NdotV * (1.0 - k) + k);
  float gL = NdotL / (NdotL * (1.0 - k) + k);
  return gV * gL;
}

// screen-space bump from a height field h(uv) in world XZ
vec3 perturbNormalWS(vec3 N, vec3 worldPos, float h, float scale) {
  vec3 dpdx = dFdx(worldPos);
  vec3 dpdy = dFdy(worldPos);
  vec3 T = normalize(dpdx);
  vec3 B = normalize(cross(N, T));
  float hx = dFdx(h);
  float hy = dFdy(h);
  vec3 Np = normalize(N - scale * (hx * B + hy * T));
  return Np;
}

// hemisphere ambient (sky/ground)
vec3 hemilight(vec3 N) {
  vec3 sky = vec3(0.55, 0.64, 0.80);
  vec3 ground = vec3(0.23, 0.20, 0.17);
  float t = saturate(N.y * 0.5 + 0.5);
  return mix(ground, sky, t) * 0.28;
}

// ============================================================================
// MAIN FRAGMENT SHADER
// ============================================================================

// ---------------------
// main
// ---------------------
void main() {
  vec3 baseColor = u_color;
  if (u_useTexture)
    baseColor *= texture(u_texture, v_texCoord).rgb;

  vec3 N = normalize(v_normal);
  vec2 uv = v_worldPos.xz * 5.0;

  float avg = (baseColor.r + baseColor.g + baseColor.b) * (1.0 / 3.0);
  float hueSpan = max(max(baseColor.r, baseColor.g), baseColor.b) -
                  min(min(baseColor.r, baseColor.g), baseColor.b);

  // === MATERIAL ID ROUTING ===
  // Rider materials
  bool isRiderSkin = (u_materialId == 0);
  bool isRiderArmor = (u_materialId == 1);
  bool isRiderHelmet = (u_materialId == 2);
  bool isRiderWeapon = (u_materialId == 3);
  bool isRiderShield = (u_materialId == 4);
  bool isRiderClothing = (u_materialId == 5);

  // Horse materials
  bool isHorseHide = (u_materialId == 6);
  bool isHorseMane = (u_materialId == 7);
  bool isHorseHoof = (u_materialId == 8);

  // Tack materials
  bool isSaddleLeather = (u_materialId == 9);
  bool isBridle = (u_materialId == 10);
  bool isSaddleBlanket = (u_materialId == 11);

  // Fallback to color-based detection if Material ID not set
  // Trust that low-position geometry is horse, not rider
  if (u_materialId == 0 && v_worldPos.y < 1.0) {
    isHorseHide = true;
  }

  // Legacy color-based detection (kept for compatibility)
  bool isBrass = (baseColor.r > baseColor.g * 1.15 &&
                  baseColor.r > baseColor.b * 1.20 && avg > 0.50);
  bool isSteel = (avg > 0.60 && !isBrass && !isHorseHide);
  bool isChain =
      (!isSteel && !isBrass && avg > 0.40 && avg <= 0.60 && !isHorseHide);
  bool isFabric =
      (!isSteel && !isBrass && !isChain && avg > 0.25 && !isHorseHide);
  bool isLeather =
      (!isSteel && !isBrass && !isChain && !isFabric && !isHorseHide);

  // lighting frame
  vec3 L = normalize(vec3(1.0, 1.2, 1.0));
  vec3 V = normalize(
      vec3(0.0, 1.0, 0.5)); // stable view proxy (keeps interface unchanged)
  vec3 H = normalize(L + V);

  float NdotL = saturate(dot(N, L));
  float NdotV = saturate(dot(N, V));
  float NdotH = saturate(dot(N, H));
  float VdotH = saturate(dot(V, H));

  // wrap diffuse like original (softens lambert)
  float wrapAmount = (avg > 0.50) ? 0.08 : 0.30;
  float NdotL_wrap = max(NdotL * (1.0 - wrapAmount) + wrapAmount, 0.12);

  // base material params
  float roughness = 0.5;
  vec3 F0 = vec3(0.04); // dielectric default
  float metalness = 0.0;
  vec3 albedo = baseColor;

  // micro details / masks (re-used)
  float nSmall = fbm(uv * 6.0);
  float nLarge = fbm(uv * 2.0);
  float cavity = 1.0 - (nLarge * 0.25 + nSmall * 0.15);

  // ---------------------
  // MATERIAL BRANCHES
  // ---------------------
  vec3 col = vec3(0.0);
  vec3 ambient = hemilight(N) * (0.85 + 0.15 * cavity);

  if (isHorseHide) {
    // =====================================================================
    // ENHANCED HORSE HIDE RENDERING (Phase 2)
    // Multi-layer procedural horse coat with anatomical accuracy
    // =====================================================================

    // Base coat color variation (bay horse: reddish-brown body)
    // Using worldspace position for consistent coat pattern across animation
    float coatNoise = fbm(v_worldPos.xz * 1.2);
    float coatVar =
        0.85 + coatNoise * 0.30; // 0.85-1.15 range for natural variation

    // Hair direction flow follows musculature (v_horseMusculature computed in
    // vertex) Hair grows from spine outward/downward
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 hairFlow = normalize(cross(up, N) + vec3(v_hairFlow * 0.3, 0.0, 0.0));

    // Individual hair texture (fine detail at 120x frequency)
    float hairTex = fbm(v_worldPos.xz * 120.0) * 0.12;

    // Coat sheen (healthy horse = glossy, following hair direction)
    float sheenFalloff = pow(saturate(dot(V, reflect(L, N))), 3.5);
    float coatSheen = sheenFalloff * (0.15 + 0.10 * coatVar);

    // Muscle definition shadows (visible on flanks, shoulders)
    // v_horseMusculature peaks at mid-torso, fades at extremities
    float muscleShadow = v_horseMusculature * 0.18;
    float shadowPattern = smoothstep(0.40, 0.70, v_horseMusculature);
    muscleShadow *= shadowPattern;

    // Dappling pattern (some horses have spotted coat on rump/shoulders)
    // Frequency 4.5x for hand-sized spots, smoothstep for soft edges
    float dappleNoise =
        fbm(v_worldPos.xz * 4.5 + vec2(v_bodyHeight * 2.0, 0.0));
    float dapples = smoothstep(0.75, 0.85, dappleNoise) * 0.14;
    // Dapples only on upper body (bodyHeight > 0.5)
    dapples *= smoothstep(0.45, 0.60, v_bodyHeight);

    // Sweat/moisture zones (lower body, under tack area)
    // Darkens coat, increases roughness
    float sweatZone = smoothstep(0.30, 0.20, v_bodyHeight);  // legs darker
    sweatZone += smoothstep(0.55, 0.65, v_bodyHeight) * 0.4; // saddle area
    float moisture = sweatZone * (0.3 + 0.2 * fbm(v_worldPos.xz * 8.0));
    moisture = saturate(moisture);

    // Dirt accumulation (legs, belly, procedural based on height)
    float dirtHeight =
        smoothstep(0.35, 0.15, v_bodyHeight); // heavy on lower legs
    float dirtPattern =
        fbm(v_worldPos.xz * 15.0 + vec2(0.0, v_bodyHeight * 5.0));
    float dirt = dirtHeight * (0.5 + 0.5 * dirtPattern) * 0.25;

    // Anisotropic highlight along hair flow (subtle, horse hair is coarser than
    // human)
    vec3 T = normalize(hairFlow);
    float flowNoise = fbm(v_worldPos.xz * 10.0);
    float anisoHighlight =
        pow(saturate(dot(H, T)), 18.0) * (0.6 + 0.4 * flowNoise);
    anisoHighlight *= 0.08 * (1.0 - moisture * 0.5); // reduced when wet

    // Composite coat color
    // Base albedo darkened by: muscle shadows, moisture, dirt
    // Lightened by: dapples, coat variation
    albedo = albedo * coatVar;
    albedo = albedo * (1.0 - muscleShadow);
    albedo = albedo * (1.0 + dapples);
    albedo = albedo * (1.0 - moisture * 0.35); // wet coat darker
    albedo = mix(albedo, albedo * 0.60, dirt); // dirt desaturates and darkens
    albedo = albedo * (1.0 + hairTex * 0.15);  // fine hair detail

    // Material properties
    // Roughness: base 0.58, smoother with sheen, rougher when wet/dirty
    roughness = 0.58 - coatSheen * 0.12 + moisture * 0.15 + dirt * 0.20;
    roughness = saturate(roughness);
    F0 = vec3(0.035); // low specular for organic hair
    metalness = 0.0;

    // Normal perturbation from hair grain (stronger than simple version)
    float hairBump = fbm(v_worldPos.xz * 35.0) + hairTex * 0.5;
    N = perturbNormalWS(N, v_worldPos, hairBump, 0.45);

    // Lighting composition
    col += ambient * albedo;

    // Microfacet specular (PBR)
    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnelSchlick(VdotH, F0);
    vec3 spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    col += NdotL_wrap * (albedo * (1.0 - F) * 0.95) + spec * 0.8;

    // Add anisotropic hair highlight and coat sheen
    col += anisoHighlight * vec3(0.9, 0.85, 0.80);    // warm highlight
    col += coatSheen * vec3(1.0, 0.98, 0.95) * NdotL; // bright sheen when lit

  } else if (isHorseMane) {
    // =====================================================================
    // HORSE MANE & TAIL RENDERING (Phase 2)
    // Coarse hair with anisotropic Kajiya-Kay lighting model
    // =====================================================================

    // Mane/tail hair is coarser than body coat (80x frequency vs 120x)
    float hairStrandNoise =
        fbm(v_worldPos.xz * 80.0 + vec2(0.0, v_worldPos.y * 15.0));
    float hairStrands = hairStrandNoise * 0.35;

    // Hair clumping (manes separate into sections naturally)
    // Clumps are 3-5cm wide, giving ~20x frequency for clump pattern
    float clumpNoise = fbm(v_worldPos.xz * 20.0);
    float clumping = smoothstep(0.35, 0.65, clumpNoise) * 0.25;

    // Flow direction (falls with gravity + movement)
    // v_hairFlow = 0.5-1.0 computed in vertex shader
    vec3 gravityDir = vec3(0.0, -1.0, 0.0);
    vec3 hairFlowDir = normalize(mix(gravityDir, N, v_hairFlow * 0.4));

    // Anisotropic Kajiya-Kay highlight (hair-specific lighting)
    // Tangent along hair flow direction
    vec3 T = normalize(cross(hairFlowDir, vec3(1.0, 0.0, 0.0)) + 1e-4);
    float TdotL = dot(T, L);
    float TdotV = dot(T, V);

    // Primary highlight (specular along hair strand)
    float primaryShift = 0.05; // slight offset for realism
    float primarySpec =
        sqrt(1.0 - TdotL * TdotL) * sqrt(1.0 - TdotV * TdotV) - TdotL * TdotV;
    primarySpec = saturate(primarySpec + primaryShift);
    primarySpec = pow(primarySpec, 18.0); // tight highlight for coarse hair

    // Secondary highlight (softer, shifted)
    float secondaryShift = 0.15;
    float secondarySpec =
        sqrt(1.0 - TdotL * TdotL) * sqrt(1.0 - TdotV * TdotV) - TdotL * TdotV;
    secondarySpec = saturate(secondarySpec + secondaryShift);
    secondarySpec = pow(secondarySpec, 8.0); // broader highlight

    // Coarse texture (mane hair is rough, 150x noise)
    float coarseTex =
        fbm(v_worldPos.xz * 150.0 + vec2(v_worldPos.y * 25.0, 0.0));
    coarseTex = coarseTex * 0.20;

    // Mane is darker than body coat (often 15% darker)
    // Start with base color, darken significantly
    albedo = albedo * 0.75; // darker base
    albedo =
        albedo *
        (1.0 - clumping * 0.3); // clumps are darker (shadow between strands)
    albedo = albedo * (1.0 + hairStrands * 0.15); // individual hair variation
    albedo = albedo * (1.0 + coarseTex * 0.10);   // coarse texture detail

    // Material properties for hair
    roughness = 0.65 + coarseTex * 0.15; // coarse hair = rough
    roughness = saturate(roughness);
    F0 = vec3(0.045); // slightly higher than body coat
    metalness = 0.0;

    // Normal perturbation from hair strands (strong directional)
    float hairBump = hairStrandNoise * 2.0 + clumpNoise * 0.8;
    N = perturbNormalWS(N, v_worldPos, hairBump, 0.65);

    // Lighting composition
    col += ambient * albedo * 0.90; // mane in partial shadow

    // Diffuse (reduced for hair)
    col += NdotL_wrap * albedo * 0.75;

    // Anisotropic highlights (Kajiya-Kay)
    col +=
        primarySpec * vec3(0.85, 0.80, 0.75) * 0.35; // warm primary highlight
    col += secondarySpec * vec3(0.95, 0.92, 0.88) * 0.18; // softer secondary

    // Add some rim light for hair strands
    float rim = pow(1.0 - NdotV, 3.0) * 0.12;
    col += rim * vec3(0.9, 0.85, 0.80);

  } else if (isHorseHoof) {
    // =====================================================================
    // HORSE HOOF RENDERING (Phase 2)
    // Dark horn keratin with growth rings and wear patterns
    // =====================================================================

    // Hooves are dark horn keratin (black/dark brown base)
    // Darken the base color significantly
    albedo = albedo * vec3(0.15, 0.12, 0.10); // very dark, slightly warm

    // Growth rings (hooves grow from coronet band downward)
    // Rings are ~1-2mm apart, giving ~45x frequency for ring pattern
    float ringHeight = v_worldPos.y * 45.0; // vertical rings
    float growthRings = abs(sin(ringHeight)) * 0.08;
    // Rings more visible near top (fresher growth)
    growthRings *= smoothstep(0.0, 0.15, v_bodyHeight);

    // Edge chipping (hooves wear down at bottom edge)
    // v_hoofWear = 0.0-0.3 computed in vertex shader
    float chipNoise = fbm(v_worldPos.xz * 55.0);
    float chipping = smoothstep(0.65, 0.85, chipNoise) * v_hoofWear;
    // Chips only at bottom edge (bodyHeight < 0.05)
    chipping *= smoothstep(0.08, 0.02, v_bodyHeight);

    // Hardened keratin sheen (polished by ground contact)
    // Bottom of hoof is smoother from wear
    float groundContact = smoothstep(0.10, 0.03, v_bodyHeight);
    float keratinSheen =
        pow(saturate(dot(V, reflect(L, N))), 15.0) * groundContact;
    keratinSheen *= 0.18;

    // Dirt/mud in crevices (procedural accumulation)
    float creviceNoise =
        fbm(v_worldPos.xz * 30.0 + vec2(0.0, v_worldPos.y * 20.0));
    float crevices = smoothstep(0.45, 0.55, creviceNoise) * 0.12;
    // More dirt at bottom
    float dirtAccum = smoothstep(0.15, 0.05, v_bodyHeight) * 0.35;
    float dirtPattern = fbm(v_worldPos.xz * 18.0);
    float dirt = dirtAccum * (0.6 + 0.4 * dirtPattern);

    // Keratin texture (fine detail, harder than hair)
    float keratinTex =
        fbm(v_worldPos.xz * 80.0 + vec2(v_worldPos.y * 35.0, 0.0));
    keratinTex *= 0.08;

    // Composite albedo
    albedo = albedo * (1.0 + growthRings); // subtle ring variation
    albedo = albedo *
             (1.0 - chipping * 0.4); // chips are darker (exposed inner hoof)
    albedo =
        mix(albedo, albedo * 1.8, dirt * 0.7); // dirt lightens (grey/brown mud)
    albedo = albedo * (1.0 + keratinTex * 0.12); // fine texture
    albedo = albedo * (1.0 + crevices * 0.15);   // crevices slightly lighter

    // Material properties for keratin
    // Keratin is hard and can be quite smooth when polished
    roughness = 0.45 - groundContact * 0.20 + dirt * 0.30 + chipping * 0.25;
    roughness = saturate(roughness);
    F0 = vec3(0.042); // moderate specular for keratin
    metalness = 0.0;

    // Normal perturbation from texture and wear
    float hoofBump =
        keratinTex * 2.0 + growthRings * 1.5 + chipNoise * chipping * 3.0;
    N = perturbNormalWS(N, v_worldPos, hoofBump, 0.55);

    // Lighting composition
    col += ambient * albedo;

    // Microfacet specular (PBR)
    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnelSchlick(VdotH, F0);
    vec3 spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    col += NdotL_wrap * (albedo * (1.0 - F)) + spec * 0.75;

    // Add keratin sheen highlight
    col += keratinSheen *
           vec3(0.95, 0.93, 0.90); // cool highlight for hard surface

  } else if (isSaddleLeather) {
    // =====================================================================
    // SADDLE LEATHER RENDERING (Phase 3)
    // Vegetable-tanned leather (4-horn Roman saddle, no stirrups)
    // =====================================================================

    // Vegetable-tanned leather base (tan/brown color)
    // Darkens with age and oil treatment
    float leatherAge = v_leatherWear * 0.4; // 0.0-0.2 darkening
    albedo = albedo * vec3(0.85, 0.68, 0.52) * (1.0 - leatherAge);

    // Leather grain texture (natural hide texture)
    // Fine grain at 40x frequency
    float grainNoise =
        fbm(v_worldPos.xz * 40.0 + vec2(v_worldPos.y * 12.0, 0.0));
    float grain = grainNoise * 0.18 * (1.0 + v_leatherWear * 0.5);

    // Tool marks (saddles were hand-carved and shaped)
    // Horizontal marks from scraping/smoothing
    float toolMarks = abs(sin(v_worldPos.x * 28.0 + grainNoise * 3.0)) * 0.06;
    toolMarks *= smoothstep(0.3, 0.7, fbm(v_worldPos.xz * 8.0));

    // Wear patterns (high contact areas become smooth and dark)
    // Seat area (mid-height) shows most wear
    float contactWear = smoothstep(0.4, 0.6, v_bodyHeight) * v_leatherWear;
    contactWear *= (0.7 + 0.3 * fbm(v_worldPos.xz * 15.0));

    // Creases and folds (leather deforms with use)
    float creasePattern = abs(sin(v_worldPos.y * 35.0 + v_worldPos.x * 8.0));
    float creases =
        smoothstep(0.85, 0.95, creasePattern) * v_leatherWear * 0.15;

    // Oil/wax darkening (Romans treated leather with oils)
    // More oil accumulation in crevices
    float oilAccum = fbm(v_worldPos.xz * 25.0) * 0.20;
    oilAccum *= (0.6 + v_leatherWear * 0.4);

    // Edge wear (corners and edges get lighter/rougher)
    float edgeNoise = fbm(v_worldPos.xz * 50.0);
    float edges = smoothstep(0.75, 0.90, edgeNoise) * 0.12;

    // Stitching hints (saddles were sewn with sinew/leather cord)
    float stitchLine =
        abs(sin(v_worldPos.x * 45.0)) * abs(sin(v_worldPos.z * 45.0));
    float stitching = smoothstep(0.92, 0.98, stitchLine) * 0.08;

    // Composite albedo
    albedo = albedo * (1.0 + grain);
    albedo = albedo * (1.0 - toolMarks * 0.5);
    albedo = albedo * (1.0 - contactWear * 0.35); // worn areas darker
    albedo = albedo * (1.0 - creases * 0.4);
    albedo = mix(albedo, albedo * 0.70, oilAccum); // oil darkens significantly
    albedo =
        albedo * (1.0 + edges * 0.3); // edges lighter (exposed fresh leather)
    albedo = albedo * (1.0 + stitching * 0.15);

    // Material properties for leather
    // Roughness increases with wear, decreases with oil
    roughness = 0.55 + v_leatherWear * 0.25 - oilAccum * 0.20;
    roughness = saturate(roughness);
    F0 = vec3(0.038); // low specular for matte leather
    metalness = 0.0;

    // Normal perturbation from grain and creases
    float leatherBump = grainNoise * 2.0 + creases * 2.5 + toolMarks * 1.5;
    N = perturbNormalWS(N, v_worldPos, leatherBump, 0.50);

    // Lighting composition
    col += ambient * albedo;

    // Microfacet specular (PBR)
    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnelSchlick(VdotH, F0);
    vec3 spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    col += NdotL_wrap * (albedo * (1.0 - F)) + spec * 0.65;

    // Subtle leather sheen in worn areas
    float leatherSheen = pow(1.0 - NdotV, 4.0) * contactWear * 0.08;
    col += leatherSheen * vec3(0.90, 0.85, 0.75);

  } else if (isBridleLeather) {
    // =====================================================================
    // BRIDLE & REINS RENDERING (Phase 3)
    // Leather straps with bronze/brass fittings
    // =====================================================================

    // Bridle leather (thinner straps, more wear than saddle)
    // Often darker dye (brown/black)
    albedo = albedo * vec3(0.45, 0.35, 0.28) * (1.0 - v_leatherWear * 0.3);

    // Leather strap texture (narrower grain pattern)
    float strapGrain =
        fbm(v_worldPos.xz * 55.0 + vec2(v_worldPos.y * 20.0, 0.0));
    strapGrain *= 0.15;

    // Stitching (bridles are carefully sewn)
    float stitchFreq = 42.0;
    float stitchX = abs(sin(v_worldPos.x * stitchFreq));
    float stitchZ = abs(sin(v_worldPos.z * stitchFreq));
    float stitches =
        max(smoothstep(0.90, 0.96, stitchX), smoothstep(0.90, 0.96, stitchZ)) *
        0.12;

    // Wear on contact points (where straps rub against horse/metal)
    float contactNoise = fbm(v_worldPos.xz * 22.0);
    float strapWear = contactNoise * v_leatherWear * 0.40;

    // Metal fittings (buckles, rings) - bronze/brass patina
    // Small regular patterns suggest metal hardware
    float metalPattern = step(0.85, fbm(v_worldPos.xz * 18.0)) * 0.25;
    vec3 bronzeColor = vec3(0.72, 0.55, 0.35); // warm bronze
    vec3 patinaColor = vec3(0.35, 0.52, 0.42); // green patina
    float patina = fbm(v_worldPos.xz * 60.0) * 0.35;
    vec3 metalColor = mix(bronzeColor, patinaColor, patina);

    // Flexibility creases (straps bend and fold)
    float flexCrease = abs(sin(v_worldPos.y * 40.0 + v_worldPos.x * 15.0));
    float creasing = smoothstep(0.80, 0.92, flexCrease) * v_leatherWear * 0.18;

    // Sweat staining (bridle contacts horse constantly)
    float sweatStain = smoothstep(0.45, 0.65, v_bodyHeight) * 0.25;
    sweatStain *= fbm(v_worldPos.xz * 12.0);

    // Composite albedo
    albedo = albedo * (1.0 + strapGrain);
    albedo = albedo * (1.0 + stitches * 0.5); // stitching lighter (thread)
    albedo = mix(albedo, albedo * 0.65, strapWear); // wear darkens
    albedo = mix(albedo, metalColor, metalPattern); // metal fittings
    albedo = albedo * (1.0 - creasing * 0.35);
    albedo = mix(albedo, albedo * 0.75, sweatStain); // sweat darkens

    // Material properties (mix leather and metal)
    float isMetalArea = metalPattern;
    roughness =
        mix(0.60 + v_leatherWear * 0.20, 0.35 + patina * 0.25, isMetalArea);
    roughness = saturate(roughness);
    F0 = mix(vec3(0.040), vec3(0.65), isMetalArea); // leather vs bronze
    metalness = isMetalArea * 0.8;

    // Normal perturbation from stitching and creases
    float bridleBump = strapGrain * 2.0 + stitches * 3.0 + creasing * 2.0;
    N = perturbNormalWS(N, v_worldPos, bridleBump, 0.55);

    // Lighting composition
    col += ambient * albedo;

    // Microfacet specular (PBR)
    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnelSchlick(VdotH, F0);
    vec3 spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    col += NdotL_wrap * (albedo * (1.0 - F)) + spec * 0.70;

    // Metal fitting highlights
    col += isMetalArea * pow(saturate(dot(N, H)), 25.0) * 0.15 *
           vec3(0.95, 0.88, 0.75);

  } else if (isSaddleBlanket) {
    // =====================================================================
    // SADDLE BLANKET RENDERING (Phase 3)
    // Wool/linen fabric (between saddle and horse back)
    // =====================================================================

    // Fabric base (wool or linen, often undyed or red)
    // Natural colors: cream, red (expensive), brown
    vec3 fabricBase = baseColor; // use base color for variety

    // Weave pattern (visible textile structure)
    // Warp and weft threads at 65x frequency
    float warpThreads = abs(sin(v_worldPos.x * 65.0));
    float weftThreads = abs(sin(v_worldPos.z * 65.0));
    float weavePattern = (warpThreads * weftThreads) * 0.18;

    // Thread thickness variation
    float threadNoise = fbm(v_worldPos.xz * 80.0);
    float threadVar = threadNoise * 0.12;

    // Color variation (natural dye inconsistency)
    float dyeVariation = fbm(v_worldPos.xz * 3.5) * 0.15;

    // Wear and pilling (fabric surface roughens with use)
    float pillingNoise = fbm(v_worldPos.xz * 45.0);
    float pilling = smoothstep(0.60, 0.80, pillingNoise) * v_leatherWear * 0.20;

    // Sweat staining (blanket absorbs horse sweat)
    // Darkens significantly in center (under saddle)
    float centerStain = smoothstep(0.45, 0.65, v_bodyHeight) * 0.40;
    float stainPattern = fbm(v_worldPos.xz * 8.0);
    float sweatStain = centerStain * (0.7 + 0.3 * stainPattern);

    // Edge fraying (blanket edges unravel over time)
    float edgeNoise = fbm(v_worldPos.xz * 55.0);
    float fraying = smoothstep(0.80, 0.95, edgeNoise) * v_leatherWear * 0.15;

    // Compression marks (saddle weight creates patterns)
    float compressionPattern = smoothstep(0.50, 0.65, v_bodyHeight);
    float compression = compressionPattern * fbm(v_worldPos.xz * 20.0) * 0.12;

    // Dirt accumulation (fabric traps dirt easily)
    float dirtPattern = fbm(v_worldPos.xz * 18.0);
    float dirt = v_leatherWear * dirtPattern * 0.30;
    vec3 dirtColor = vec3(0.32, 0.28, 0.24);

    // Composite albedo
    albedo = fabricBase;
    albedo = albedo * (1.0 + dyeVariation);
    albedo = albedo * (1.0 + weavePattern);
    albedo = albedo * (1.0 + threadVar);
    albedo = albedo * (1.0 + pilling * 0.5);
    albedo =
        mix(albedo, albedo * 0.60, sweatStain); // sweat darkens significantly
    albedo = albedo * (1.0 - fraying * 0.4);
    albedo = albedo * (1.0 - compression * 0.3);
    albedo = mix(albedo, dirtColor, dirt); // dirt discolors fabric

    // Material properties for fabric
    // Fabric is very matte, roughness increases with wear
    roughness = 0.75 + pilling * 0.15 + sweatStain * 0.10;
    roughness = saturate(roughness);
    F0 = vec3(0.028); // very low specular for fabric
    metalness = 0.0;

    // Normal perturbation from weave and pilling
    float fabricBump = weavePattern * 2.5 + pilling * 2.0 + threadNoise * 1.5;
    N = perturbNormalWS(N, v_worldPos, fabricBump, 0.60);

    // Lighting composition
    col += ambient * albedo * 1.05; // fabric reflects ambient well

    // Diffuse dominant for fabric (minimal specular)
    col += NdotL_wrap * albedo * 0.95;

    // Very subtle specular for wet/compressed areas
    float fabricSheen = sweatStain * compression * 0.05;
    col += fabricSheen * vec3(0.85, 0.82, 0.78);

  } else if (isRiderArmor) {
    // =====================================================================
    // CAVALRY ARMOR RENDERING (Phase 4)
    // Scale armor (lorica squamata) and muscle cuirass variations
    // Elite swordsmen have higher chance of muscle cuirass
    // =====================================================================

    // Determine armor type by body height and unit variation
    // Swordsmen (elite) have higher armorSheen, more likely muscle cuirass
    bool isMuscleCuirass =
        (v_bodyHeight > 0.55 && v_bodyHeight < 0.75 && v_armorSheen > 0.75);

    if (isMuscleCuirass) {
      // === MUSCLE CUIRASS (Anatomical Bronze) ===
      // Elite cavalry officers wore anatomically shaped bronze cuirasses

      // Bronze base with anatomical muscle definition
      vec3 bronzeBase = vec3(0.72, 0.58, 0.42) * baseColor;

      // Anatomical muscle shapes (pectorals, abs, ribs)
      // Vertical ribbing for torso muscles
      float muscleY = v_worldPos.y * 12.0;
      float musclePattern = abs(sin(muscleY)) * abs(cos(muscleY * 0.5));
      float muscleDef = smoothstep(0.40, 0.70, musclePattern) *
                        0.22; // Elite: stronger definition

      // Horizontal bands for ab definition
      float abBands =
          abs(sin(v_worldPos.y * 18.0)) * 0.14;        // Elite: more prominent
      abBands *= smoothstep(0.58, 0.68, v_bodyHeight); // abs mid-torso

      // Edge definition (cuirass borders)
      float edgeNoise = fbm(v_worldPos.xz * 35.0);
      float edges = smoothstep(0.80, 0.92, edgeNoise) * 0.16;

      // Decorative elements (medusa heads, eagles, wreaths)
      float decorPattern =
          step(0.85, fbm(v_worldPos.xz * 8.0)) * 0.20; // Elite: more decoration
      decorPattern *= smoothstep(0.60, 0.65, v_bodyHeight); // chest decoration

      // Bronze patina (less on elite - better maintained)
      float patinaPattern = fbm(v_worldPos.xz * 5.5);
      float patina = patinaPattern * 0.22;       // Elite: less patina
      vec3 patinaColor = vec3(0.35, 0.52, 0.42); // verdigris green

      // Battle damage (dents, scratches)
      float dentNoise = fbm(v_worldPos.xz * 12.0);
      float dents =
          smoothstep(0.78, 0.88, dentNoise) * 0.12; // Elite: less damage

      // Leather strapping (for fastening)
      float strapPattern =
          abs(sin(v_worldPos.x * 8.0)) * abs(sin(v_worldPos.z * 45.0));
      float straps = smoothstep(0.92, 0.98, strapPattern) * 0.12;
      vec3 leatherColor = vec3(0.35, 0.28, 0.22);

      // Composite albedo
      albedo = bronzeBase;
      albedo = albedo * (1.0 + muscleDef); // highlight muscle ridges
      albedo = albedo * (1.0 + abBands);
      albedo = albedo * (1.0 + edges * 0.5);
      albedo =
          albedo * (1.0 + decorPattern * 0.7); // Elite: brighter decoration
      albedo = mix(albedo, patinaColor, patina);
      albedo = albedo * (1.0 - dents * 0.4);            // dents darker
      albedo = mix(albedo, leatherColor, straps * 0.3); // strap visibility

      // Material properties for polished bronze (elite: higher polish)
      roughness = 0.18 + patina * 0.30 + dents * 0.12; // Elite: smoother
      roughness = saturate(roughness);
      F0 = vec3(0.70); // Elite: slightly higher specular
      metalness = 0.96;

      // Normal perturbation from muscle definition
      float cuirassBump = muscleDef * 3.2 + abBands * 2.8 + decorPattern * 2.2;
      N = perturbNormalWS(N, v_worldPos, cuirassBump, 0.62);

    } else {
      // === SCALE ARMOR (Lorica Squamata) ===
      // Overlapping bronze/iron scales sewn onto fabric backing

      // Individual scale geometry (small overlapping scales)
      // Scales are ~2.5cm wide, giving ~30x frequency
      float scaleX = v_worldPos.x * 30.0;
      float scaleY = v_worldPos.y * 30.0;

      // Offset every other row for brick pattern
      float rowOffset = mod(floor(scaleY), 2.0) * 0.5;
      scaleX += rowOffset;

      // Individual scale shape (rounded rectangle)
      vec2 scaleUV = fract(vec2(scaleX, scaleY)) - 0.5;
      float scaleDist = max(abs(scaleUV.x) * 1.2, abs(scaleUV.y)) - 0.35;
      float scale = smoothstep(0.05, -0.05, scaleDist);

      // Overlapping pattern (scales cover each other)
      float overlapPattern = smoothstep(0.4, 0.5, fract(scaleY));
      float overlap = scale * overlapPattern * 0.25;

      // Scale edge lighting (rim light on scale edges)
      float scaleEdge = smoothstep(-0.02, 0.02, abs(scaleDist)) * scale;
      float edgeLighting = scaleEdge * pow(saturate(dot(N, L)), 2.0) * 0.35;

      // Metal variation per scale (some scales more worn/oxidized)
      float scaleVariation = hash(floor(vec2(scaleX, scaleY)));
      float scalePatina = scaleVariation * 0.25;

      // Bronze/iron mix (cavalry scale armor often mixed metals)
      bool isIronScale = (scaleVariation > 0.65);
      vec3 bronzeColor = vec3(0.72, 0.58, 0.42);
      vec3 ironColor = vec3(0.60, 0.60, 0.62);
      vec3 metalColor = mix(bronzeColor, ironColor, step(0.65, scaleVariation));

      // Verdigris patina on bronze scales
      vec3 patinaColor = vec3(0.35, 0.52, 0.42);
      metalColor = mix(metalColor, patinaColor,
                       scalePatina * (1.0 - float(isIronScale)));

      // Fabric backing visible between scales
      float fabricGap = (1.0 - scale) * 0.40;
      vec3 fabricColor = vec3(0.42, 0.38, 0.35); // dark fabric

      // Battle damage (missing/bent scales)
      float damagePattern = fbm(v_worldPos.xz * 8.0);
      float damage = smoothstep(0.82, 0.92, damagePattern) * 0.20;

      // Composite albedo
      albedo = metalColor * scale;                  // scale metal color
      albedo = mix(albedo, fabricColor, fabricGap); // fabric between scales
      albedo = albedo * (1.0 + overlap);      // overlapping scales brighter
      albedo = albedo * (1.0 - damage * 0.5); // damage darkens
      albedo = albedo * baseColor * 0.7;      // tint with base color

      // Material properties (metal scales on fabric)
      float metalArea = scale * (1.0 - damage);
      roughness = mix(0.75, 0.32 + scalePatina * 0.30, metalArea);
      roughness = saturate(roughness);
      F0 = mix(vec3(0.030), vec3(0.65), metalArea);
      metalness = metalArea * 0.85;

      // Normal perturbation from scale geometry
      float scaleHeight = scale * 2.0 + overlap * 1.5;
      N = perturbNormalWS(N, v_worldPos, scaleHeight, 0.70);

      // Add edge lighting to final color later
      albedo = albedo * (1.0 + edgeLighting);
    }

    // Common lighting for both armor types
    col += ambient * albedo * 0.85; // armor in partial shadow

    // Microfacet specular (PBR)
    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnelSchlick(VdotH, F0);
    vec3 spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    col += NdotL_wrap * (albedo * (1.0 - F) * 0.60 +
                         spec * 1.30); // Elite: slightly better spec

    // Additional metallic rim light
    float armorRim = pow(1.0 - NdotV, 4.0) * 0.16; // Elite: brighter rim
    col += armorRim * vec3(0.95, 0.90, 0.82);

  } else if (isHelmet) {
    // =====================================================================
    // HELMET RENDERING (Phase 4)
    // Attic, Phrygian, and Thracian style variations
    // =====================================================================

    // Helmet style determined by height (different parts)
    bool isCrestHolder = (v_bodyHeight > 0.85);      // top of helmet
    bool isCheekGuard = (v_bodyHeight < 0.30);       // lower face protection
    bool isBowl = (!isCrestHolder && !isCheekGuard); // main helmet bowl

    // Bronze helmet base
    vec3 bronzeBase = vec3(0.72, 0.58, 0.42) * baseColor;

    if (isBowl) {
      // === HELMET BOWL (Main Protection) ===

      // Hammered bronze texture
      float hammerMarks = fbm(v_worldPos.xz * 25.0) * 0.12;

      // Decorative ribbing (strengthen helmet structure)
      float ribbingPattern = abs(sin(v_worldPos.y * 20.0 + v_worldPos.x * 5.0));
      float ribbing = smoothstep(0.88, 0.94, ribbingPattern) * 0.15;

      // Bronze patina (oxidation)
      float patinaPattern = fbm(v_worldPos.xz * 6.0);
      float patina = patinaPattern * 0.25; // Elite: less patina
      vec3 patinaColor = vec3(0.35, 0.52, 0.42);

      // Battle damage (dents from blows)
      float dentPattern = fbm(v_worldPos.xz * 10.0);
      float dents =
          smoothstep(0.80, 0.90, dentPattern) * 0.15; // Elite: less damage

      // Polished vs worn areas
      float polishPattern = smoothstep(0.60, 0.75, v_bodyHeight);
      float polish = polishPattern * (1.0 - patina * 0.5);

      // Composite albedo
      albedo = bronzeBase;
      albedo = albedo * (1.0 + hammerMarks);
      albedo = albedo * (1.0 + ribbing * 0.6);
      albedo = mix(albedo, patinaColor, patina);
      albedo = albedo * (1.0 - dents * 0.35);

      roughness = 0.22 - polish * 0.12 + patina * 0.32 +
                  dents * 0.18; // Elite: smoother

    } else if (isCheekGuard) {
      // === CHEEK GUARDS (Hinged Face Protection) ===

      // Thinner bronze, more flexible
      float guardTexture = fbm(v_worldPos.xz * 35.0) * 0.10;

      // Hinge rivets
      float rivetPattern = step(0.90, fbm(v_worldPos.xz * 22.0)) * 0.15;

      // More patina (close to face, more moisture)
      float guardPatina =
          fbm(v_worldPos.xz * 7.0) * 0.32; // Elite: slightly less
      vec3 patinaColor = vec3(0.35, 0.52, 0.42);

      // Scratches from wear
      float scratches = abs(sin(v_worldPos.x * 60.0)) * 0.08;

      albedo = bronzeBase;
      albedo = albedo * (1.0 + guardTexture);
      albedo = albedo * (1.0 + rivetPattern * 0.4);
      albedo = mix(albedo, patinaColor, guardPatina);
      albedo = albedo * (1.0 - scratches * 0.5);

      roughness = 0.30 + guardPatina * 0.35; // Elite: smoother

    } else { // isCrestHolder
      // === CREST HOLDER (Top Ridge) ===

      // Reinforced ridge for plume attachment
      float ridgePattern = abs(sin(v_worldPos.x * 15.0)) * 0.18;

      // Decorative engraving
      float engraving = fbm(v_worldPos.xz * 12.0) * 0.16; // Elite: more detail

      // Less patina (exposed to air, drier)
      float topPatina = fbm(v_worldPos.xz * 5.0) * 0.18; // Elite: even less
      vec3 patinaColor = vec3(0.35, 0.52, 0.42);

      albedo = bronzeBase;
      albedo = albedo * (1.0 + ridgePattern);
      albedo = albedo * (1.0 + engraving * 0.6);
      albedo = mix(albedo, patinaColor, topPatina);

      roughness = 0.25 + topPatina * 0.28; // Elite: smoother
    }

    // Common helmet properties
    roughness = saturate(roughness);
    F0 = vec3(0.68); // Elite: slightly higher specular
    metalness = 0.94;

    // Normal perturbation
    float helmetBump = fbm(v_worldPos.xz * 28.0) * 2.0;
    N = perturbNormalWS(N, v_worldPos, helmetBump, 0.55);

    // Lighting composition
    col += ambient * albedo * 0.82; // Elite: slightly brighter

    // Microfacet specular
    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnelSchlick(VdotH, F0);
    vec3 spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    col += NdotL_wrap *
           (albedo * (1.0 - F) * 0.55 + spec * 1.40); // Elite: better spec

    // Bronze rim light
    float helmetRim = pow(1.0 - NdotV, 3.5) * 0.20; // Elite: brighter rim
    col += helmetRim * vec3(0.95, 0.88, 0.75);

  } else if (isSteel) {
    float brushed =
        abs(sin(v_worldPos.y * 95.0)) * 0.02 + noise(uv * 35.0) * 0.015;
    float dents = noise(uv * 8.0) * 0.03;
    float plates = armorPlates(v_worldPos.xz, v_worldPos.y);

    // bump from brushing
    float h = fbm(vec2(v_worldPos.y * 25.0, v_worldPos.z * 6.0));
    N = perturbNormalWS(N, v_worldPos, h, 0.35);

    // steel-like params
    metalness = 1.0;
    F0 = vec3(0.92);
    roughness = 0.28 + brushed * 2.0 + dents * 0.6;
    roughness = clamp(roughness, 0.15, 0.55);

    // base tint & sky reflection lift
    albedo = mix(vec3(0.60), baseColor, 0.25);
    float skyRefl = (N.y * 0.5 + 0.5) * 0.10;

    // microfacet spec only for metals
    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnelSchlick(VdotH, F0 * albedo); // slight tint
    vec3 spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    col += ambient * 0.3; // metals rely more on spec
    col += NdotL_wrap * spec * 1.5;
    col += vec3(plates) + vec3(skyRefl) - vec3(dents * 0.25) + vec3(brushed);

  } else if (isBrass) {
    float brassNoise = noise(uv * 22.0) * 0.02;
    float patina = fbm(uv * 4.0) * 0.12; // larger-scale patina

    // bump from subtle hammering
    float h = fbm(uv * 18.0);
    N = perturbNormalWS(N, v_worldPos, h, 0.30);

    metalness = 1.0;
    vec3 brassTint = vec3(0.94, 0.78, 0.45);
    F0 = mix(brassTint, baseColor, 0.5);
    roughness = clamp(0.32 + patina * 0.45, 0.18, 0.75);

    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnelSchlick(VdotH, F0);
    vec3 spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    col += ambient * 0.25;
    col += NdotL_wrap * spec * 1.35;
    col += vec3(brassNoise) - vec3(patina * 0.35);

  } else if (isChain) {
    float rings = chainmailRings(v_worldPos.xz);
    float ringHi = noise(uv * 30.0) * 0.10;

    // small pitted bump
    float h = fbm(uv * 35.0);
    N = perturbNormalWS(N, v_worldPos, h, 0.25);

    metalness = 1.0;
    F0 = vec3(0.86);
    roughness = 0.35;

    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnelSchlick(VdotH, F0);
    vec3 spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    col += ambient * 0.25;
    col += NdotL_wrap * (spec * (1.2 + rings)) + vec3(ringHi);
    // slight diffuse damping to keep chainmail darker in cavities
    col *= (0.95 - 0.10 * (1.0 - cavity));

  } else if (isFabric) {
    float weaveX = sin(v_worldPos.x * 70.0);
    float weaveZ = sin(v_worldPos.z * 70.0);
    float weave = weaveX * weaveZ * 0.04;
    float embroidery = fbm(uv * 6.0) * 0.08;

    float h = fbm(uv * 22.0) * 0.7 + weave * 0.6;
    N = perturbNormalWS(N, v_worldPos, h, 0.35);

    roughness = 0.78;
    F0 = vec3(0.035);
    metalness = 0.0;

    vec3 F = fresnelSchlick(VdotH, F0);
    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    float sheen = pow(1.0 - NdotV, 6.0) * 0.10;
    albedo *= 1.0 + fbm(uv * 5.0) * 0.10 - 0.05;

    col += ambient * albedo;
    col += NdotL_wrap * (albedo * (1.0 - F) + spec * 0.3) +
           vec3(weave + embroidery + sheen);

  } else { // leather
    float grain = fbm(uv * 10.0) * 0.15;
    float wear = fbm(uv * 3.0) * 0.12;

    float h = fbm(uv * 18.0);
    N = perturbNormalWS(N, v_worldPos, h, 0.28);

    roughness = 0.58 - wear * 0.15;
    F0 = vec3(0.038);
    metalness = 0.0;

    vec3 F = fresnelSchlick(VdotH, F0);
    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    float sheen = pow(1.0 - NdotV, 4.0) * 0.06;

    albedo *= (1.0 + grain - 0.06 + wear * 0.05);
    col += ambient * albedo;
    col += NdotL_wrap * (albedo * (1.0 - F)) + spec * 0.4 + vec3(sheen);
  }

  // final clamp and alpha
  col = saturate(col);
  FragColor = vec4(col, u_alpha);
}
