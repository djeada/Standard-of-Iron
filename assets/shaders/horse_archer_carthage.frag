#version 330 core

// ============================================================================
// CARTHAGINIAN HORSE ARCHER - Rich Brown Leather & Team-Colored Cloak
// Unique earth-tone palette with vibrant team colors
// ============================================================================

in vec3 v_normal;
in vec2 v_texCoord;
in vec3 v_worldPos;
in float v_armorLayer;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_useTexture;
uniform float u_alpha;
uniform int u_materialId;

out vec4 FragColor;

// ============================================================================
// CONSTANTS & HELPERS
// ============================================================================

const float PI = 3.14159265359;

// *** Core bases
const vec3 LEATHER_BASE_BROWN = vec3(0.36, 0.22, 0.10);
const vec3 BRONZE_BASE_COLOR  = vec3(0.86, 0.66, 0.36);
const vec3 ARCHER_SKIN_BASE    = vec3(0.08, 0.07, 0.065);

float saturate(float x) { return clamp(x, 0.0, 1.0); }
vec3 saturate3(vec3 x) { return clamp(x, 0.0, 1.0); }

vec3 boostSaturation(vec3 color, float amount) {
  float grey = dot(color, vec3(0.299, 0.587, 0.114));
  return mix(vec3(grey), color, 1.0 + amount);
}

float hash2(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float a = hash2(i);
  float b = hash2(i + vec2(1.0, 0.0));
  float c = hash2(i + vec2(0.0, 1.0));
  float d = hash2(i + vec2(1.0, 1.0));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
  float v = 0.0;
  float a = 0.5;
  mat2 rot = mat2(0.87, 0.50, -0.50, 0.87);
  for (int i = 0; i < 5; ++i) {
    v += a * noise(p);
    p = rot * p * 2.0 + vec2(100.0);
    a *= 0.5;
  }
  return v;
}

// ============================================================================
// PBR FUNCTIONS
// ============================================================================

float D_GGX(float NdotH, float roughness) {
  float a = roughness * roughness;
  float a2 = a * a;
  float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
  return a2 / (PI * denom * denom + 1e-6);
}

float G_SchlickGGX(float NdotX, float roughness) {
  float r = roughness + 1.0;
  float k = (r * r) / 8.0;
  return NdotX / (NdotX * (1.0 - k) + k + 1e-6);
}

float G_Smith(float NdotV, float NdotL, float roughness) {
  return G_SchlickGGX(NdotV, roughness) * G_SchlickGGX(NdotL, roughness);
}

vec3 F_Schlick(float cosTheta, vec3 F0) {
  float t = 1.0 - cosTheta;
  float t5 = t * t * t * t * t;
  return F0 + (1.0 - F0) * t5;
}

// ============================================================================
// LEATHER TEXTURE - RICH BROWN PALETTE
// ============================================================================

vec3 brownLeatherPalette(float variation) {
  vec3 darkBrown = vec3(0.28, 0.18, 0.10);
  vec3 redBrown  = vec3(0.45, 0.25, 0.15);
  vec3 warmBrown = vec3(0.55, 0.38, 0.22);
  vec3 lightTan  = vec3(0.68, 0.52, 0.35);
  
  if (variation < 0.33) {
    return mix(darkBrown, redBrown, variation * 3.0);
  } else if (variation < 0.66) {
    return mix(redBrown, warmBrown, (variation - 0.33) * 3.0);
  } else {
    return mix(warmBrown, lightTan, (variation - 0.66) * 3.0);
  }
}

float leatherGrain(vec2 uv, float scale) {
  float coarse = fbm(uv * scale);
  float medium = fbm(uv * scale * 2.5 + 7.3);
  float fine = noise(uv * scale * 6.0);
  float pores = smoothstep(0.55, 0.65, noise(uv * scale * 4.0));
  return coarse * 0.4 + medium * 0.35 + fine * 0.15 + pores * 0.1;
}

float stitchPattern(vec2 uv, float spacing) {
  float stitch = fract(uv.y * spacing);
  stitch = smoothstep(0.4, 0.5, stitch) * smoothstep(0.6, 0.5, stitch);
  float seamLine = smoothstep(0.48, 0.50, fract(uv.x * 4.0)) * 
                   smoothstep(0.52, 0.50, fract(uv.x * 4.0));
  return stitch * seamLine;
}

float battleWear(vec3 pos) {
  float scratch1 = smoothstep(0.72, 0.77, noise(pos.xy * 22.0 + pos.z * 4.0));
  float scratch2 = smoothstep(0.74, 0.79, noise(pos.zy * 18.0 - 2.5));
  float scuff = fbm(pos.xz * 7.0) * fbm(pos.xy * 10.0);
  scuff = smoothstep(0.35, 0.55, scuff);
  return (scratch1 + scratch2) * 0.25 + scuff * 0.35;
}

float oilSheen(vec3 pos, vec3 N, vec3 V) {
  float facing = 1.0 - abs(dot(N, V));
  float variation = fbm(pos.xz * 12.0) * 0.5 + 0.5;
  return facing * facing * variation;
}

// ============================================================================
// CLOAK & FABRIC
// ============================================================================

float cloakFolds(vec3 pos) {
  float folds = sin(pos.x * 8.0 + pos.y * 3.0) * 0.5 + 0.5;
  folds += sin(pos.z * 6.0 - pos.y * 2.0) * 0.3;
  float wind = fbm(pos.xz * 4.0 + pos.y * 2.0);
  return folds * 0.6 + wind * 0.4;
}

float fabricWeave(vec2 uv) {
  float warpX = sin(uv.x * 100.0) * 0.5 + 0.5;
  float weftY = sin(uv.y * 100.0) * 0.5 + 0.5;
  return warpX * weftY;
}

// ============================================================================
// HORSE PATTERNS
// ============================================================================

float horseCoatPattern(vec2 uv) {
  float coarse = fbm(uv * 3.0) * 0.15;
  float fine = noise(uv * 25.0) * 0.08;
  float dapple = smoothstep(0.4, 0.6, fbm(uv * 5.0)) * 0.1;
  return coarse + fine + dapple;
}

float furStrand(vec2 uv, float density) {
  float strand = sin(uv.x * density) * cos(uv.y * density * 0.7);
  strand = strand * 0.5 + 0.5;
  float variation = noise(uv * density * 0.5);
  return strand * 0.3 + variation * 0.2;
}

// ============================================================================
// BOW WOOD
// ============================================================================

float woodGrainBow(vec3 pos) {
  float grain = sin(pos.y * 40.0 + fbm(pos.xy * 8.0) * 3.0);
  grain = grain * 0.5 + 0.5;
  float rings = fbm(vec2(pos.x * 20.0, pos.z * 20.0));
  return grain * 0.7 + rings * 0.3;
}

// ============================================================================
// MAIN
// ============================================================================

void main() {
  vec3 baseColor = clamp(u_color, 0.0, 1.0);
  if (u_useTexture) {
    baseColor *= texture(u_texture, v_texCoord).rgb;
  }
  
  // Material IDs
  bool is_rider_skin      = (u_materialId == 0);
  bool is_armor           = (u_materialId == 1);
  bool is_helmet          = (u_materialId == 2);
  bool is_weapon          = (u_materialId == 3);
  bool is_shield          = (u_materialId == 4);
  bool is_rider_clothing  = (u_materialId == 5);
  bool is_horse_hide      = (u_materialId == 6);
  bool is_horse_mane      = (u_materialId == 7);
  bool is_horse_hoof      = (u_materialId == 8);
  bool is_saddle_leather  = (u_materialId == 9);
  bool is_bridle          = (u_materialId == 10);
  bool is_saddle_blanket  = (u_materialId == 11);
  bool is_cloak           = (u_materialId == 12 || u_materialId == 13);
  
  vec3 N = normalize(v_normal);
  vec3 V = normalize(vec3(0.0, 0.5, 1.0));
  vec3 L = normalize(vec3(0.5, 1.0, 0.4));
  vec3 H = normalize(L + V);
  
  vec3 albedo = baseColor;
  float metallic = 0.0;
  float roughness = 0.5;
  float sheen = 0.0;
  
  // ========== MATERIAL SETUP ==========
  
  if (is_rider_skin) {
    vec3 skinBase = ARCHER_SKIN_BASE;
    vec3 teamTint = mix(skinBase, baseColor, 0.2);
    float toneNoise = fbm(v_worldPos.xz * 3.1) - 0.5;
    albedo = clamp(teamTint + vec3(toneNoise) * 0.04, 0.0, 1.0);
    metallic = 0.0;
    roughness = 0.58;
    
  } else if (is_armor) {
    // ====== RICH BROWN LEATHER ARMOR (UV-based, clearly rough leather) ======
    vec2 leatherUV = v_texCoord * 6.0;                         // *** UV-driven
    float leatherVar = fbm(leatherUV * 2.0);
    vec3 leatherPalette = brownLeatherPalette(leatherVar);
    
    // *** Core brown leather base, lightly tinted by team color
    vec3 tint = mix(vec3(1.0), baseColor, 0.25);
    vec3 leatherBase = mix(LEATHER_BASE_BROWN, leatherPalette, 0.5);
    albedo = leatherBase * tint;
    albedo = boostSaturation(albedo, 0.25);
    
    float grain    = leatherGrain(leatherUV, 8.0);              // *** stronger grain
    float wear     = battleWear(v_worldPos);
    float oil      = oilSheen(v_worldPos, N, V);
    float stitches = stitchPattern(leatherUV, 18.0);            // *** visible seams
    
    // Strong grain contrast
    float grainStrength = 0.55;
    albedo = mix(albedo * 0.7, albedo * 1.3, grain * grainStrength);
    
    // Darker creases / lighter raised areas
    float depth = fbm(leatherUV * 3.0);
    albedo = mix(albedo * 0.7, albedo * 1.15, depth);
    
    // Worn edges lighter & slightly desaturated
    vec3 wornColor = albedo * 1.3 + vec3(0.1, 0.08, 0.05);
    wornColor = mix(wornColor, vec3(dot(wornColor, vec3(0.3, 0.59, 0.11))), 0.15);
    albedo = mix(albedo, wornColor, wear * 0.6);
    
    // Stitch darkening
    albedo = mix(albedo, albedo * 0.55, stitches * 0.9);
    
    // Leather straps (keep existing pattern)
    float straps = smoothstep(0.45, 0.48, fract(v_worldPos.x * 6.0)) *
                   smoothstep(0.55, 0.52, fract(v_worldPos.x * 6.0));
    vec3 strapColor = brownLeatherPalette(0.15);
    albedo = mix(albedo, strapColor, straps * 0.6);
    
    // *** Fully leather-like roughness
    metallic  = 0.0;
    float baseRough = 0.85;
    roughness = baseRough - oil * 0.25 + grain * 0.05;
    roughness = clamp(roughness, 0.65, 0.9);
    sheen     = oil * 0.6;
    
  } else if (is_helmet) {
    // ====== BRONZE-HEAVY HELMET WITH SMALL LEATHER UNDER-CAP ======
    
    // Leather under-cap near bottom/inside
    float leatherZone = smoothstep(0.2, 0.0, v_worldPos.y);
    leatherZone *= smoothstep(0.25, 0.15, abs(v_worldPos.x));
    
    float helmLeatherVar = fbm(v_worldPos.xz * 6.0);
    vec3 helmLeather = brownLeatherPalette(helmLeatherVar * 0.6 + 0.2);
    float helmGrain = leatherGrain(v_worldPos.xz, 12.0);
    helmLeather *= 0.9 + helmGrain * 0.2;
    
    // *** Hammered bronze shell
    vec2 bronzeUV   = v_texCoord * 10.0;
    float bronzeNoise = fbm(bronzeUV * 2.5);
    float hammer      = noise(bronzeUV * 20.0);
    
    vec3 bronzeColor = BRONZE_BASE_COLOR;
    vec3 patina      = vec3(0.78, 0.82, 0.70);
    bronzeColor = mix(bronzeColor, patina, bronzeNoise * 0.25);
    bronzeColor *= 0.9 + hammer * 0.25;
    bronzeColor = boostSaturation(bronzeColor, 0.2);
    
    // Extra bright bronze trim on cheek guards and ridge
    float trimMask = 0.0;
    trimMask += smoothstep(0.65, 0.8, abs(v_worldPos.x) * 1.8); // sides
    trimMask += smoothstep(0.8, 0.9, v_worldPos.y);             // top
    trimMask = saturate(trimMask);
    
    vec3 trimBronze = bronzeColor * 1.25 + vec3(0.06, 0.04, 0.02);
    
    // Combine bronze + trim, with small leather zone
    vec3 helmBase = mix(bronzeColor, trimBronze, trimMask * 0.7);
    albedo = mix(helmLeather, helmBase, 1.0 - leatherZone * 0.7);
    
    // Strongly metallic bronze, non-metallic leather
    float bronzeMetal  = 0.95;
    float leatherMetal = 0.0;
    metallic = mix(bronzeMetal, leatherMetal, leatherZone);
    
    // Bronze relatively smooth, leather rough
    float bronzeRough = 0.32 + hammer * 0.05;
    float leatherRough = 0.7;
    roughness = mix(bronzeRough, leatherRough, leatherZone);
    
    sheen = (1.0 - leatherZone) * 0.35;
    
  } else if (is_weapon) {
    // ====== BOW ======
    float h = v_worldPos.y;
    float grip = smoothstep(0.38, 0.45, h) * smoothstep(0.62, 0.55, h);
    float limbs = 1.0 - grip;
    float stringArea = smoothstep(0.92, 1.0, h) + smoothstep(0.08, 0.0, h);
    
    float woodGrain = woodGrainBow(v_worldPos);
    vec3 woodLight = vec3(0.72, 0.55, 0.35);
    vec3 woodDark = vec3(0.48, 0.32, 0.18);
    vec3 woodColor = mix(woodDark, woodLight, woodGrain);
    woodColor = boostSaturation(woodColor, 0.2);
    
    float woodPolish = pow(max(dot(reflect(-V, N), L), 0.0), 24.0);
    
    float gripGrain = leatherGrain(v_worldPos.xy, 25.0);
    vec3 gripLeather = brownLeatherPalette(0.25);
    gripLeather *= 0.9 + gripGrain * 0.2;
    
    float wrapPattern = sin(v_worldPos.y * 50.0 + v_worldPos.x * 20.0);
    wrapPattern = smoothstep(-0.2, 0.2, wrapPattern);
    gripLeather *= 0.9 + wrapPattern * 0.15;
    
    vec3 stringColor = vec3(0.85, 0.80, 0.72);
    float stringShine = pow(max(dot(reflect(-V, N), L), 0.0), 48.0);
    
    albedo = woodColor;
    albedo = mix(albedo, gripLeather, grip);
    albedo = mix(albedo, stringColor, stringArea * 0.8);
    
    metallic = 0.0;
    roughness = mix(0.38, 0.48, grip);
    roughness = mix(roughness, 0.32, stringArea);
    sheen = woodPolish * limbs * 0.4 + grip * 0.3 + stringShine * stringArea * 0.3;
    
  } else if (is_shield) {
    float leatherVar = fbm(v_worldPos.xz * 5.0);
    vec3 leatherColor = brownLeatherPalette(leatherVar * 0.7 + 0.15);
    
    float grain = leatherGrain(v_worldPos.xz, 10.0);
    leatherColor *= 0.88 + grain * 0.24;
    
    float dist = length(v_worldPos.xz);
    float boss = smoothstep(0.15, 0.0, dist);
    float edgeRim = smoothstep(0.85, 0.95, dist);
    
    vec3 bronzeColor = vec3(0.85, 0.65, 0.35);
    bronzeColor = boostSaturation(bronzeColor, 0.25);
    
    albedo = leatherColor;
    albedo = mix(albedo, bronzeColor, boss + edgeRim * 0.7);
    
    metallic = mix(0.0, 0.9, boss + edgeRim * 0.6);
    roughness = mix(0.48, 0.22, boss);
    sheen = (1.0 - boss) * (1.0 - edgeRim) * 0.35;
    
  } else if (is_rider_clothing) {
    // Linen tunic
    float weave = fabricWeave(v_worldPos.xy);
    float threadVar = noise(v_worldPos.xz * 40.0);
    
    albedo = boostSaturation(baseColor, 0.2);
    albedo *= 0.92 + weave * 0.08 + threadVar * 0.06;
    
    float folds = fbm(v_worldPos.xy * 6.0);
    albedo *= 0.88 + folds * 0.18;
    
    metallic = 0.0;
    roughness = 0.72;
    sheen = pow(1.0 - max(dot(N, V), 0.0), 4.0) * 0.12;
    
  } else if (is_cloak) {
    // ====== TEAM-COLORED CLOAK ======
    vec3 teamColor = boostSaturation(baseColor, 0.5);
    teamColor *= 1.2;
    
    float folds = cloakFolds(v_worldPos);
    float weave = fabricWeave(v_worldPos.xy);
    
    albedo = teamColor;
    albedo *= 0.75 + folds * 0.5;
    albedo *= 0.95 + weave * 0.08;
    
    vec3 shadowTint = mix(teamColor, teamColor * vec3(0.85, 0.9, 1.0), 0.3);
    albedo = mix(shadowTint, albedo, folds);
    
    float edgeFray = noise(v_worldPos.xy * 40.0);
    albedo *= 0.97 + edgeFray * 0.06;
    
    metallic = 0.0;
    roughness = 0.72;
    sheen = pow(1.0 - max(dot(N, V), 0.0), 4.0) * 0.25;
    
  } else if (is_horse_hide) {
    // ====== HORSE COAT ======
    float coat = horseCoatPattern(v_worldPos.xz);
    float fur = furStrand(v_worldPos.xz, 60.0);
    
    albedo = boostSaturation(baseColor, 0.18);
    albedo *= 0.9 + coat * 0.15 + fur * 0.1;
    
    float muscle = fbm(v_worldPos.xy * 4.0);
    albedo *= 0.95 + muscle * 0.1;
    
    metallic = 0.0;
    roughness = 0.65;
    sheen = 0.22;
    
  } else if (is_horse_mane) {
    float strand = furStrand(v_worldPos.xy, 120.0);
    float clump = fbm(v_worldPos.xy * 8.0);
    
    albedo = baseColor * 0.35;
    albedo = boostSaturation(albedo, 0.15);
    albedo *= 0.85 + strand * 0.2 + clump * 0.1;
    
    metallic = 0.0;
    roughness = 0.75;
    sheen = 0.18;
    
  } else if (is_horse_hoof) {
    float grain = fbm(v_worldPos.xy * 20.0);
    
    albedo = vec3(0.18, 0.15, 0.12);
    albedo = boostSaturation(albedo, 0.1);
    albedo *= 0.9 + grain * 0.15;
    
    metallic = 0.0;
    roughness = 0.45;
    sheen = 0.28;
    
  } else if (is_saddle_leather || is_bridle) {
    // ====== TACK - Rich brown leather ======
    float leatherVar = fbm(v_worldPos.xy * 5.0);
    vec3 tackLeather = brownLeatherPalette(leatherVar * 0.5 + 0.3);
    
    float grain = leatherGrain(v_worldPos.xy, 18.0);
    float wear = battleWear(v_worldPos) * 0.5;
    float oil = oilSheen(v_worldPos, N, V);
    
    albedo = tackLeather;
    albedo = boostSaturation(albedo, 0.28);
    albedo *= 0.88 + grain * 0.22;
    
    vec3 wornColor = albedo * 1.2 + vec3(0.06, 0.04, 0.02);
    albedo = mix(albedo, wornColor, wear * 0.4);
    
    if (is_bridle) {
      float buckle = smoothstep(0.78, 0.82, noise(v_worldPos.xz * 15.0));
      vec3 brassColor = vec3(0.82, 0.62, 0.32);
      albedo = mix(albedo, brassColor, buckle * 0.85);
      metallic = mix(0.0, 0.85, buckle);
      roughness = mix(0.42, 0.28, buckle);
    } else {
      metallic = 0.0;
      roughness = 0.42 - oil * 0.12 + grain * 0.08;
    }
    
    sheen = oil * 0.45;
    
  } else if (is_saddle_blanket) {
    // Woven wool - can have team color accent
    float weaveX = sin(v_worldPos.x * 50.0);
    float weaveZ = sin(v_worldPos.z * 50.0);
    float weave = weaveX * weaveZ * 0.5 + 0.5;
    float fuzz = noise(v_worldPos.xz * 80.0);
    
    vec3 woolBase = vec3(0.45, 0.35, 0.25);
    vec3 teamStripe = boostSaturation(baseColor, 0.4);
    
    float stripe = smoothstep(0.35, 0.45, fract(v_worldPos.x * 3.0));
    stripe *= smoothstep(0.65, 0.55, fract(v_worldPos.x * 3.0));
    
    albedo = mix(woolBase, teamStripe, stripe * 0.7);
    albedo = boostSaturation(albedo, 0.2);
    albedo *= 0.88 + weave * 0.12 + fuzz * 0.06;
    
    metallic = 0.0;
    roughness = 0.82;
    sheen = 0.08;
    
  } else {
    // Default - leather-tinted
    float leatherVar = fbm(v_worldPos.xy * 6.0);
    albedo = mix(brownLeatherPalette(leatherVar), baseColor, 0.4);
    albedo = boostSaturation(albedo, 0.15);
    metallic = 0.0;
    roughness = 0.55;
  }
  
  // ========== PBR LIGHTING ==========
  
  float NdotL = max(dot(N, L), 0.0);
  float NdotV = max(dot(N, V), 0.001);
  float NdotH = max(dot(N, H), 0.0);
  float VdotH = max(dot(V, H), 0.0);
  
  vec3 F0 = mix(vec3(0.04), albedo, metallic);
  
  float D = D_GGX(NdotH, max(roughness, 0.01));
  float G = G_Smith(NdotV, NdotL, roughness);
  vec3 F = F_Schlick(VdotH, F0);
  vec3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 0.001);
  
  vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
  vec3 diffuse = kD * albedo / PI;
  
  vec3 color = (diffuse + specular * 1.6) * NdotL * 2.0;
  
  // ====== SHEEN EFFECTS ======
  if (sheen > 0.01) {
    vec3 R = reflect(-V, N);
    float sheenSpec = pow(max(dot(R, L), 0.0), 14.0);
    color += albedo * sheenSpec * sheen * 1.4;
    
    float edgeSheen = pow(1.0 - NdotV, 3.0);
    color += vec3(0.92, 0.88, 0.78) * edgeSheen * sheen * 0.35;
  }
  
  // ====== METALLIC SHINE (bronze) ======
  if (metallic > 0.5) {
    vec3 R = reflect(-V, N);
    float specPower = 128.0 / (roughness * roughness + 0.001);
    specPower = min(specPower, 512.0);
    float mirrorSpec = pow(max(dot(R, L), 0.0), specPower);
    color += albedo * mirrorSpec * 1.1;
    
    float hotspot = pow(NdotH, 256.0);
    color += vec3(1.0) * hotspot * (1.0 - roughness);
  }
  
  // ====== WARM AMBIENT ======
  vec3 ambient = albedo * 0.36;
  
  float sss = pow(saturate(dot(-N, L)), 2.5) * 0.12;
  ambient += albedo * vec3(1.1, 0.92, 0.75) * sss;
  
  float rim = pow(1.0 - NdotV, 3.5);
  ambient += vec3(0.32, 0.30, 0.26) * rim * 0.22;
  
  color += ambient;
  
  // Tone mapping
  color = color / (color + vec3(0.55));
  color = pow(color, vec3(0.94));
  
  FragColor = vec4(saturate3(color), u_alpha);
}
