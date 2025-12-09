#version 330 core

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

const float PI = 3.14159265359;

const vec3 LEATHER_BASE_BROWN = vec3(0.36, 0.22, 0.10);
const vec3 BRONZE_BASE_COLOR = vec3(0.86, 0.66, 0.36);

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

vec3 brownLeatherPalette(float variation) {
  vec3 darkBrown = vec3(0.28, 0.18, 0.10);
  vec3 redBrown = vec3(0.45, 0.25, 0.15);
  vec3 warmBrown = vec3(0.55, 0.38, 0.22);
  vec3 lightTan = vec3(0.68, 0.52, 0.35);

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
  float seamLine = smoothstep(0.48, 0.50, fract(uv.x * 3.0)) *
                   smoothstep(0.52, 0.50, fract(uv.x * 3.0));
  return stitch * seamLine;
}

float battleWear(vec3 pos) {
  float scratch1 = smoothstep(0.7, 0.75, noise(pos.xy * 25.0 + pos.z * 5.0));
  float scratch2 = smoothstep(0.72, 0.77, noise(pos.zy * 20.0 - 3.7));
  float scuff = fbm(pos.xz * 8.0) * fbm(pos.xy * 12.0);
  scuff = smoothstep(0.3, 0.5, scuff);
  float edgeWear = smoothstep(0.4, 0.8, pos.y) * fbm(pos.xz * 6.0);
  return (scratch1 + scratch2) * 0.3 + scuff * 0.4 + edgeWear * 0.3;
}

float oilSheen(vec3 pos, vec3 N, vec3 V) {
  float facing = 1.0 - abs(dot(N, V));
  float variation = fbm(pos.xz * 15.0) * 0.5 + 0.5;
  return facing * facing * variation;
}

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

void main() {
  vec3 baseColor = clamp(u_color, 0.0, 1.0);
  if (u_useTexture) {
    baseColor *= texture(u_texture, v_texCoord).rgb;
  }
  baseColor = boostSaturation(baseColor, 0.25);

  bool is_rider_skin = (u_materialId == 0);
  bool is_armor = (u_materialId == 1);
  bool is_helmet = (u_materialId == 2);
  bool is_weapon = (u_materialId == 3);
  bool is_shield = (u_materialId == 4);
  bool is_rider_clothing = (u_materialId == 5);
  bool is_horse_hide = (u_materialId == 6);
  bool is_horse_mane = (u_materialId == 7);
  bool is_horse_hoof = (u_materialId == 8);
  bool is_saddle_leather = (u_materialId == 9);
  bool is_bridle = (u_materialId == 10);
  bool is_saddle_blanket = (u_materialId == 11);

  vec3 N = normalize(v_normal);
  vec3 V = normalize(vec3(0.0, 0.5, 1.0));
  vec3 L = normalize(vec3(0.5, 1.0, 0.4));
  vec3 H = normalize(L + V);

  vec3 albedo = baseColor;
  float metallic = 0.0;
  float roughness = 0.5;
  float sheen = 0.0;

  if (is_rider_skin) {

    albedo = mix(baseColor, vec3(0.75, 0.58, 0.45), 0.3);
    albedo = boostSaturation(albedo, 0.15);
    metallic = 0.0;
    roughness = 0.55;

  } else if (is_armor) {

    vec2 leatherUV = v_texCoord * 6.0;
    float leatherVar = fbm(leatherUV * 2.0);
    vec3 leatherPalette = brownLeatherPalette(leatherVar);

    vec3 tint = mix(vec3(1.0), baseColor, 0.25);
    vec3 leatherBase = mix(LEATHER_BASE_BROWN, leatherPalette, 0.5);
    albedo = leatherBase * tint;
    albedo = boostSaturation(albedo, 0.25);

    float grain = leatherGrain(leatherUV, 8.0);
    float wear = battleWear(v_worldPos);
    float oil = oilSheen(v_worldPos, N, V);
    float stitches = stitchPattern(leatherUV, 18.0);

    float grainStrength = 0.55;
    albedo = mix(albedo * 0.7, albedo * 1.3, grain * grainStrength);

    float depth = fbm(leatherUV * 3.0);
    albedo = mix(albedo * 0.7, albedo * 1.15, depth);

    vec3 wornColor = albedo * 1.3 + vec3(0.1, 0.08, 0.05);
    wornColor =
        mix(wornColor, vec3(dot(wornColor, vec3(0.3, 0.59, 0.11))), 0.15);
    albedo = mix(albedo, wornColor, wear * 0.6);

    albedo = mix(albedo, albedo * 0.55, stitches * 0.9);

    float straps = smoothstep(0.45, 0.48, fract(v_worldPos.x * 6.0)) *
                   smoothstep(0.55, 0.52, fract(v_worldPos.x * 6.0));
    vec3 strapColor = brownLeatherPalette(0.15);
    albedo = mix(albedo, strapColor, straps * 0.6);

    metallic = 0.0;
    float baseRough = 0.85;
    roughness = baseRough - oil * 0.25 + grain * 0.05;
    roughness = clamp(roughness, 0.65, 0.9);
    sheen = oil * 0.6;

  } else if (is_helmet) {

    float leatherZone = smoothstep(0.2, 0.0, v_worldPos.y);
    leatherZone *= smoothstep(0.25, 0.15, abs(v_worldPos.x));

    float helmLeatherVar = fbm(v_worldPos.xz * 6.0);
    vec3 helmLeather = brownLeatherPalette(helmLeatherVar * 0.6 + 0.2);
    float helmGrain = leatherGrain(v_worldPos.xz, 12.0);
    helmLeather *= 0.9 + helmGrain * 0.2;

    vec2 bronzeUV = v_texCoord * 10.0;
    float bronzeNoise = fbm(bronzeUV * 2.5);
    float hammer = noise(bronzeUV * 20.0);

    vec3 bronzeColor = BRONZE_BASE_COLOR;
    vec3 patina = vec3(0.78, 0.82, 0.70);
    bronzeColor = mix(bronzeColor, patina, bronzeNoise * 0.25);
    bronzeColor *= 0.9 + hammer * 0.25;
    bronzeColor = boostSaturation(bronzeColor, 0.2);

    float trimMask = 0.0;
    trimMask += smoothstep(0.65, 0.8, abs(v_worldPos.x) * 1.8);
    trimMask += smoothstep(0.8, 0.9, v_worldPos.y);
    trimMask = saturate(trimMask);

    vec3 trimBronze = bronzeColor * 1.25 + vec3(0.06, 0.04, 0.02);

    vec3 helmBase = mix(bronzeColor, trimBronze, trimMask * 0.7);
    albedo = mix(helmLeather, helmBase, 1.0 - leatherZone * 0.7);

    float bronzeMetal = 0.95;
    float leatherMetal = 0.0;
    metallic = mix(bronzeMetal, leatherMetal, leatherZone);

    float bronzeRough = 0.32 + hammer * 0.05;
    float leatherRough = 0.7;
    roughness = mix(bronzeRough, leatherRough, leatherZone);

    sheen = (1.0 - leatherZone) * 0.35;

  } else if (is_weapon) {

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

  } else if (is_rider_clothing) {

    float weave = sin(v_worldPos.x * 80.0) * sin(v_worldPos.z * 80.0);
    weave = weave * 0.5 + 0.5;
    float threadVar = noise(v_worldPos.xz * 40.0);

    albedo = boostSaturation(baseColor, 0.25);
    albedo *= 0.92 + weave * 0.08 + threadVar * 0.06;

    float folds = fbm(v_worldPos.xy * 6.0);
    albedo *= 0.9 + folds * 0.15;

    metallic = 0.0;
    roughness = 0.72;
    sheen = pow(1.0 - max(dot(N, V), 0.0), 4.0) * 0.15;

  } else if (is_horse_hide) {

    float coat = horseCoatPattern(v_worldPos.xz);
    float fur = furStrand(v_worldPos.xz, 60.0);

    albedo = boostSaturation(baseColor, 0.2);
    albedo *= 0.9 + coat * 0.15 + fur * 0.1;

    float muscle = fbm(v_worldPos.xy * 4.0);
    albedo *= 0.95 + muscle * 0.1;

    metallic = 0.0;
    roughness = 0.65;
    sheen = 0.25;

  } else if (is_horse_mane) {
    float strand = furStrand(v_worldPos.xy, 120.0);
    float clump = fbm(v_worldPos.xy * 8.0);

    albedo = baseColor * 0.35;
    albedo = boostSaturation(albedo, 0.15);
    albedo *= 0.85 + strand * 0.2 + clump * 0.1;

    metallic = 0.0;
    roughness = 0.75;
    sheen = 0.2;

  } else if (is_horse_hoof) {
    float grain = fbm(v_worldPos.xy * 20.0);

    albedo = vec3(0.18, 0.15, 0.12);
    albedo = boostSaturation(albedo, 0.1);
    albedo *= 0.9 + grain * 0.15;

    metallic = 0.0;
    roughness = 0.45;
    sheen = 0.3;

  } else if (is_saddle_leather || is_bridle) {

    float grain = leatherGrain(v_worldPos.xy, 18.0);
    float wear = battleWear(v_worldPos) * 0.6;
    float oil = oilSheen(v_worldPos, N, V);

    albedo = baseColor * 1.1;
    albedo = boostSaturation(albedo, 0.3);
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

    sheen = oil * 0.5;

  } else if (is_saddle_blanket) {

    float weaveX = sin(v_worldPos.x * 50.0);
    float weaveZ = sin(v_worldPos.z * 50.0);
    float weave = weaveX * weaveZ * 0.5 + 0.5;
    float fuzz = noise(v_worldPos.xz * 80.0);

    albedo = boostSaturation(baseColor, 0.35);
    albedo *= 0.88 + weave * 0.12 + fuzz * 0.06;

    float stripe = smoothstep(0.4, 0.5, fract(v_worldPos.x * 4.0));
    stripe *= smoothstep(0.6, 0.5, fract(v_worldPos.x * 4.0));
    albedo = mix(albedo, albedo * 0.7, stripe * 0.3);

    metallic = 0.0;
    roughness = 0.82;
    sheen = 0.1;

  } else {
    albedo = boostSaturation(baseColor, 0.2);
    metallic = 0.0;
    roughness = 0.55;
  }

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

  if (sheen > 0.01) {
    vec3 R = reflect(-V, N);
    float sheenSpec = pow(max(dot(R, L), 0.0), 12.0);
    color += albedo * sheenSpec * sheen * 1.5;

    float edgeSheen = pow(1.0 - NdotV, 3.0);
    color += vec3(0.95, 0.90, 0.80) * edgeSheen * sheen * 0.4;
  }

  if (metallic > 0.5) {
    vec3 R = reflect(-V, N);
    float specPower = 128.0 / (roughness * roughness + 0.001);
    specPower = min(specPower, 512.0);
    float mirrorSpec = pow(max(dot(R, L), 0.0), specPower);
    color += albedo * mirrorSpec * 1.2;

    float hotspot = pow(NdotH, 256.0);
    color += vec3(1.0) * hotspot * (1.0 - roughness);
  }

  vec3 ambient = albedo * 0.38;

  float sss = pow(saturate(dot(-N, L)), 2.0) * 0.12;
  ambient += albedo * vec3(1.1, 0.9, 0.7) * sss;

  float rim = pow(1.0 - NdotV, 3.5);
  ambient += vec3(0.35, 0.32, 0.28) * rim * 0.25;

  color += ambient;

  color = color / (color + vec3(0.55));
  color = pow(color, vec3(0.94));

  FragColor = vec4(saturate3(color), u_alpha);
}
