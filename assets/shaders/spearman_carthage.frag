#version 330 core

// ============================================================================
// CARTHAGINIAN SPEARMAN - Rich Leather Armor with Battle-Worn Character
// ============================================================================

in vec3 v_worldNormal;
in vec3 v_worldPos;
in vec2 v_texCoord;

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
const vec3 LEATHER_BROWN = vec3(0.36, 0.22, 0.10); // *** fixed realistic brown

float saturate(float x) { return clamp(x, 0.0, 1.0); }
vec3 saturate3(vec3 x) { return clamp(x, 0.0, 1.0); }

// Boost saturation
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
// LEATHER TEXTURE PATTERNS
// ============================================================================

// Natural leather grain - irregular pores and creases
float leatherGrain(vec2 uv, float scale) {
  float coarse = fbm(uv * scale);
  float medium = fbm(uv * scale * 2.5 + 7.3);
  float fine = noise(uv * scale * 6.0);
  
  // Create pore-like depressions
  float pores = smoothstep(0.55, 0.65, noise(uv * scale * 4.0));
  
  return coarse * 0.4 + medium * 0.35 + fine * 0.15 + pores * 0.1;
}

// Stitching pattern for leather seams
float stitchPattern(vec2 uv, float spacing) {
  float stitch = fract(uv.y * spacing);
  stitch = smoothstep(0.4, 0.5, stitch) * smoothstep(0.6, 0.5, stitch);
  
  // Only show stitches along certain lines
  float seamLine = smoothstep(0.48, 0.50, fract(uv.x * 3.0)) * 
                   smoothstep(0.52, 0.50, fract(uv.x * 3.0));
  return stitch * seamLine;
}

// Battle wear - scratches, scuffs, worn edges
float battleWear(vec3 pos) {
  float scratch1 = smoothstep(0.7, 0.75, noise(pos.xy * 25.0 + pos.z * 5.0));
  float scratch2 = smoothstep(0.72, 0.77, noise(pos.zy * 20.0 - 3.7));
  float scuff = fbm(pos.xz * 8.0) * fbm(pos.xy * 12.0);
  scuff = smoothstep(0.3, 0.5, scuff);
  float edgeWear = smoothstep(0.4, 0.8, pos.y) * fbm(pos.xz * 6.0);
  return (scratch1 + scratch2) * 0.3 + scuff * 0.4 + edgeWear * 0.3;
}

// Oiled leather sheen pattern
float oilSheen(vec3 pos, vec3 N, vec3 V) {
  float facing = 1.0 - abs(dot(N, V));
  float variation = fbm(pos.xz * 15.0) * 0.5 + 0.5;
  return facing * facing * variation;
}

// ============================================================================
// MAIN
// ============================================================================

void main() {
  // Get and enhance base color
  vec3 baseColor = clamp(u_color, 0.0, 1.0);
  if (u_useTexture) {
    baseColor *= texture(u_texture, v_texCoord).rgb;
  }
  baseColor = boostSaturation(baseColor, 0.25);
  
  bool is_skin   = (u_materialId == 0);
  bool is_armor  = (u_materialId == 1);
  bool is_helmet = (u_materialId == 2);
  bool is_weapon = (u_materialId == 3);
  bool is_shield = (u_materialId == 4);
  
  vec3 N = normalize(v_worldNormal);
  vec3 V = normalize(vec3(0.0, 0.0, 1.0));
  vec3 L = normalize(vec3(0.5, 1.0, 0.4));
  vec3 H = normalize(L + V);
  
  vec3 albedo = baseColor;
  float metallic = 0.0;
  float roughness = 0.5;
  float sheen = 0.0;
  
  // ========== MATERIAL SETUP ==========
  
  if (is_skin) {
    // Warm Mediterranean skin
    albedo = mix(baseColor, vec3(0.88, 0.72, 0.58), 0.25);
    albedo = boostSaturation(albedo, 0.15);
    metallic = 0.0;
    roughness = 0.55;
    
    // Leather leg wrappings
    float legWrap = 1.0 - smoothstep(0.30, 0.55, v_worldPos.y);
    float wrapGrain = leatherGrain(v_worldPos.xz, 18.0);
    vec3 wrapColor = baseColor * 0.7;
    wrapColor = boostSaturation(wrapColor, 0.2);
    wrapColor -= vec3(0.06) * wrapGrain;
    
    float bands = sin(v_worldPos.y * 25.0) * 0.5 + 0.5;
    bands = smoothstep(0.3, 0.7, bands);
    wrapColor *= 0.9 + bands * 0.15;
    
    albedo = mix(albedo, wrapColor, legWrap);
    roughness = mix(roughness, 0.48, legWrap);
    
  } else if (is_armor || is_helmet) {
    // ====== RICH LEATHER ARMOR ======
    
    // *** Use UVs so grain is visible and tiles nicely
    vec2 leatherUV = v_texCoord * 6.0; 
    
    float grain     = leatherGrain(leatherUV, 8.0);          // *** UV-based grain
    float stitches  = stitchPattern(leatherUV, 18.0);        // *** UV-based seams
    float wear      = battleWear(v_worldPos);
    float oil       = oilSheen(v_worldPos, N, V);
    
    // *** Force a realistic brown leather base, only slightly tinted by u_color
    vec3 tint = mix(vec3(1.0), baseColor, 0.25);
    albedo = LEATHER_BROWN * tint;                           // *** core leather color
    albedo = boostSaturation(albedo, 0.25);
    
    // *** Stronger grain contrast
    float grainStrength = 0.55;
    albedo = mix(albedo * 0.7, albedo * 1.3, grain * grainStrength);
    
    // Darker in creases, lighter on raised areas
    float depth = fbm(leatherUV * 3.0);                      // *** use UV for consistency
    albedo = mix(albedo * 0.7, albedo * 1.15, depth);
    
    // Worn areas are lighter/more faded + slightly desaturated
    vec3 wornColor = albedo * 1.25 + vec3(0.07, 0.05, 0.03);
    wornColor = mix(wornColor, vec3(dot(wornColor, vec3(0.3,0.59,0.11))), 0.15);
    albedo = mix(albedo, wornColor, wear * 0.65);
    
    // Stitching detail (darker seams)
    albedo = mix(albedo, albedo * 0.55, stitches * 0.9);
    
    // Leather is not metallic but quite rough
    metallic = 0.0;
    
    // *** Full leather roughness, only slightly reduced where oiled
    float baseRoughness = 0.85;
    roughness = baseRoughness - oil * 0.25 + grain * 0.05;
    roughness = clamp(roughness, 0.65, 0.9);                 // *** always quite rough
    
    // Sheen mainly from oil
    sheen = oil * 0.6;
    
    // Bronze studs/reinforcements on helmet
    if (is_helmet) {
      float studs = smoothstep(0.75, 0.8, noise(v_worldPos.xz * 12.0));
      vec3 bronzeColor = vec3(0.85, 0.65, 0.35);
      albedo = mix(albedo, bronzeColor, studs * 0.8);
      metallic = mix(metallic, 0.9, studs);
      roughness = mix(roughness, 0.25, studs);
    }
    
  } else if (is_weapon) {
    // Spear with wooden shaft and iron tip
    float h = v_worldPos.y;
    float tip = smoothstep(0.40, 0.55, h);
    float binding = smoothstep(0.35, 0.42, h) * (1.0 - tip);
    
    vec3 woodColor = boostSaturation(baseColor * 0.85, 0.3);
    float woodGrain = fbm(vec2(v_worldPos.x * 8.0, v_worldPos.y * 35.0));
    woodColor *= 0.85 + woodGrain * 0.3;
    float woodSheen = pow(max(dot(reflect(-V, N), L), 0.0), 16.0);
    
    vec3 bindColor = baseColor * 0.6;
    bindColor = boostSaturation(bindColor, 0.2);
    float bindGrain = leatherGrain(v_worldPos.xy, 25.0);
    bindColor *= 0.9 + bindGrain * 0.2;
    
    vec3 ironColor = vec3(0.55, 0.55, 0.58);
    float ironBrush = fbm(v_worldPos.xy * 40.0);
    ironColor += vec3(0.08) * ironBrush;
    ironColor = mix(ironColor, baseColor * 0.3 + vec3(0.4), 0.15);
    
    albedo = woodColor;
    albedo = mix(albedo, bindColor, binding);
    albedo = mix(albedo, ironColor, tip);
    
    metallic = mix(0.0, 0.85, tip);
    roughness = mix(0.38, 0.28, tip);
    roughness = mix(roughness, 0.5, binding);
    sheen = woodSheen * (1.0 - tip) * (1.0 - binding) * 0.3;
    
  } else if (is_shield) {
    // Leather-covered wooden shield with bronze boss
    float dist = length(v_worldPos.xz);
    float boss = smoothstep(0.18, 0.0, dist);
    float bossRim = smoothstep(0.22, 0.18, dist) * (1.0 - boss);
    
    float shieldGrain = leatherGrain(v_worldPos.xz, 10.0);
    float shieldWear = battleWear(v_worldPos);
    
    albedo = boostSaturation(baseColor, 0.3);
    albedo *= 0.9 + shieldGrain * 0.2;
    albedo = mix(albedo, albedo * 1.2, shieldWear * 0.3);
    
    vec3 bronzeColor = vec3(0.88, 0.68, 0.38);
    bronzeColor = boostSaturation(bronzeColor, 0.25);
    albedo = mix(albedo, bronzeColor, boss + bossRim * 0.8);
    
    metallic = mix(0.0, 0.9, boss + bossRim * 0.7);
    roughness = mix(0.45, 0.22, boss);
    
  } else {
    albedo = boostSaturation(baseColor, 0.2);
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
  
  vec3 color = (diffuse + specular * 1.8) * NdotL * 2.0;
  
  // ====== LEATHER SHEEN EFFECT ======
  if (sheen > 0.01) {
    vec3 R = reflect(-V, N);
    float sheenSpec = pow(max(dot(R, L), 0.0), 12.0);
    color += albedo * sheenSpec * sheen * 1.5;
    float edgeSheen = pow(1.0 - NdotV, 3.0);
    color += vec3(0.95, 0.90, 0.80) * edgeSheen * sheen * 0.4;
  }
  
  // ====== METALLIC SHINE (for bronze parts) ======
  if (metallic > 0.5) {
    vec3 R = reflect(-V, N);
    float specPower = 128.0 / (roughness * roughness + 0.001);
    specPower = min(specPower, 512.0);
    float mirrorSpec = pow(max(dot(R, L), 0.0), specPower);
    color += albedo * mirrorSpec * 1.2;
    float hotspot = pow(NdotH, 256.0);
    color += vec3(1.0) * hotspot * (1.0 - roughness);
  }
  
  // ====== WARM AMBIENT ======
  vec3 ambient = albedo * 0.38;
  float sss = pow(saturate(dot(-N, L)), 2.0) * 0.15;
  ambient += albedo * vec3(1.1, 0.9, 0.7) * sss;
  float rim = pow(1.0 - NdotV, 3.5);
  ambient += vec3(0.35, 0.32, 0.28) * rim * 0.25;
  
  color += ambient;
  
  // Tone mapping
  color = color / (color + vec3(0.55));
  color = pow(color, vec3(0.94));
  
  FragColor = vec4(saturate3(color), u_alpha);
}
