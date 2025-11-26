#version 330 core
in vec3 v_worldPos;
in vec3 v_normal;
in vec2 v_uv;
in float v_disp;
layout(location = 0) out vec4 FragColor;
uniform vec3 u_grassPrimary;
uniform vec3 u_grassSecondary;
uniform vec3 u_grassDry;
uniform vec3 u_soilColor;
uniform vec3 u_tint;
uniform vec2 u_noiseOffset;
uniform float u_tileSize;
uniform float u_macroNoiseScale;
uniform float u_detailNoiseScale;
uniform float u_soilBlendHeight;
uniform float u_soilBlendSharpness;
uniform float u_ambientBoost;
uniform vec3 u_lightDir;

// Ground-type-specific uniforms
uniform float u_snowCoverage;    // 0-1: snow accumulation
uniform float u_moistureLevel;   // 0-1: wetness/dryness
uniform float u_crackIntensity;  // 0-1: ground cracking
uniform float u_rockExposure;    // 0-1: rock visibility
uniform float u_grassSaturation; // 0-1.5: grass color intensity
uniform float u_soilRoughness;   // 0-1: soil texture roughness
uniform vec3 u_snowColor;        // Snow tint color

float hash21(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}
float noise21(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  float a = hash21(i), b = hash21(i + vec2(1, 0)), c = hash21(i + vec2(0, 1)),
        d = hash21(i + vec2(1, 1));
  vec2 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}
float fbm(vec2 p) {
  float v = 0.0, a = 0.5;
  for (int i = 0; i < 3; ++i) {
    v += noise21(p) * a;
    p = p * 2.07 + 13.17;
    a *= 0.5;
  }
  return v;
}

void main() {
  vec3 n = normalize(v_normal);
  float heightTint = clamp((v_worldPos.y + 0.6) * 0.6, -0.25, 0.35);
  float ts = max(u_tileSize, 1e-4);
  vec2 wuv = (v_worldPos.xz / ts) + u_noiseOffset;
  float macro = fbm(wuv * u_macroNoiseScale);
  float detail = noise21(wuv * (u_detailNoiseScale * 2.0));
  float patchNoise = fbm(wuv * u_macroNoiseScale * 0.4);
  float moistureVar = smoothstep(0.3, 0.7, patchNoise);
  float lush = smoothstep(0.2, 0.8, macro);
  lush = mix(lush, moistureVar, 0.3);
  vec3 lushGrass = mix(u_grassPrimary, u_grassSecondary, lush);
  float dryness = clamp(0.3 * detail, 0.0, 0.4);
  dryness += moistureVar * 0.15;
  vec3 grassCol = mix(lushGrass, u_grassDry, dryness);
  float sw = max(0.01, 1.0 / max(u_soilBlendSharpness, 1e-3));
  float sN = (noise21(wuv * 4.0 + 9.7) - 0.5) * sw * 0.8;
  float soilMix = 1.0 - smoothstep(u_soilBlendHeight - sw + sN,
                                   u_soilBlendHeight + sw + sN, v_worldPos.y);
  soilMix = clamp(soilMix, 0.0, 1.0);
  float mudPatch = fbm(wuv * 0.08 + vec2(7.3, 11.2));
  mudPatch = smoothstep(0.65, 0.75, mudPatch);
  soilMix = max(soilMix, mudPatch * 0.85);
  vec3 baseCol = mix(grassCol, u_soilColor, soilMix);

  // Ground cracking effect (for dry Mediterranean terrain)
  if (u_crackIntensity > 0.01) {
    float crackNoise1 = noise21(wuv * 8.0);
    float crackNoise2 = noise21(wuv * 16.0 + vec2(42.0, 17.0));
    float crackPattern = smoothstep(0.45, 0.50, crackNoise1) *
                         smoothstep(0.40, 0.55, crackNoise2);
    float crackDarkening = 1.0 - crackPattern * u_crackIntensity * 0.35;
    baseCol *= crackDarkening;
  }

  // Snow coverage effect (for alpine terrain)
  if (u_snowCoverage > 0.01) {
    float snowNoise = fbm(wuv * 0.5 + vec2(123.0, 456.0));
    float snowAccumulation = smoothstep(0.3, 0.7, snowNoise);
    float heightSnowBonus = smoothstep(-0.5, 1.5, v_worldPos.y) * 0.3;
    float snowMask =
        clamp(snowAccumulation * (u_snowCoverage + heightSnowBonus), 0.0, 1.0);
    vec3 snowTinted = u_snowColor * (1.0 + detail * 0.1);
    baseCol = mix(baseCol, snowTinted, snowMask * 0.85);
  }

  // Apply grass saturation modifier
  vec3 grayLevel = vec3(dot(baseCol, vec3(0.299, 0.587, 0.114)));
  baseCol = mix(grayLevel, baseCol, u_grassSaturation);

  // Moisture effect
  float wetDarkening = 1.0 - u_moistureLevel * 0.15;
  baseCol *= wetDarkening;

  vec3 dx = dFdx(v_worldPos), dy = dFdy(v_worldPos);
  float mScale = u_detailNoiseScale * 8.0 / ts;
  float h0 = noise21(wuv * mScale);
  float hx = noise21((wuv + dx.xz * mScale));
  float hy = noise21((wuv + dy.xz * mScale));
  vec2 g = vec2(hx - h0, hy - h0);
  vec3 t = normalize(dx - n * dot(n, dx));
  vec3 b = normalize(cross(n, t));
  float microAmp = 0.12;
  vec3 nMicro = normalize(n - (t * g.x + b * g.y) * microAmp);
  float jitterAmp = 0.06 * (0.5 + u_soilRoughness * 0.5);
  float jitter = (hash21(wuv * 0.27 + vec2(17.0, 9.0)) - 0.5) * jitterAmp;
  float brightnessVar = (moistureVar - 0.5) * 0.08;
  vec3 col = baseCol * (1.0 + jitter + brightnessVar);
  col *= u_tint;
  vec3 L = normalize(u_lightDir);
  float ndl = max(dot(nMicro, L), 0.0);
  float ambient = 0.40;
  float fres = pow(1.0 - max(dot(nMicro, vec3(0, 1, 0)), 0.0), 2.0);
  // Surface roughness affects specular - wet surfaces are shinier
  float surfaceRoughness = mix(0.65, 0.95, u_soilRoughness);
  surfaceRoughness = mix(surfaceRoughness, 0.45, u_moistureLevel * 0.5);
  float specContrib = fres * 0.08 * (1.0 - surfaceRoughness);
  specContrib += u_moistureLevel * 0.06 * fres;
  float shade = ambient + ndl * 0.65 + specContrib;
  vec3 lit = col * shade * (u_ambientBoost + heightTint);

  // Debug tint to make displacement obvious: blue for negative, red for
  // positive
  vec3 dispTint = mix(vec3(0.3, 0.5, 1.0), vec3(1.0, 0.4, 0.3),
                      smoothstep(-1.0, 1.0, v_disp));
  lit = mix(lit, lit * dispTint, 0.25);

  FragColor = vec4(clamp(lit, 0.0, 1.0), 1.0);
}
