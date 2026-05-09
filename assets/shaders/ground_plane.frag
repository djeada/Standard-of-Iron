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
uniform vec3 u_rockLow;
uniform vec3 u_rockHigh;
uniform vec3 u_tint;
uniform vec2 u_noiseOffset;
uniform float u_noiseAngle;
uniform float u_tileSize;
uniform float u_macroNoiseScale;
uniform float u_detailNoiseScale;
uniform float u_soilBlendHeight;
uniform float u_soilBlendSharpness;
uniform float u_ambientBoost;
uniform vec3 u_lightDir;

uniform float u_snowCoverage;
uniform float u_moistureLevel;
uniform float u_crackIntensity;
uniform float u_rockExposure;
uniform float u_grassSaturation;
uniform float u_soilRoughness;
uniform float u_microBumpAmp;
uniform float u_microBumpFreq;
uniform float u_microNormalWeight;
uniform float u_albedoJitter;
uniform vec3 u_snowColor;
uniform vec3 u_cameraPos;
uniform vec3 u_fogColor;
uniform float u_fogStart;
uniform float u_fogEnd;

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

mat2 rot2(float a) {
  float c = cos(a), s = sin(a);
  return mat2(c, -s, s, c);
}

void main() {
  vec3 n = normalize(v_normal);
  float ts = max(u_tileSize, 1e-4);
  float slope = 1.0 - clamp(n.y, 0.0, 1.0);
  float heightTint = clamp((v_worldPos.y + 0.35) * 0.55, -0.22, 0.30);

  vec2 baseUv = rot2(u_noiseAngle) * ((v_worldPos.xz / ts) + u_noiseOffset);
  vec2 warpNoise =
      vec2(fbm(baseUv * 0.22), fbm((baseUv + vec2(13.7, -6.1)) * 0.22));
  vec2 wuv = baseUv + (warpNoise - 0.5) * 1.1;

  float macro = fbm(wuv * u_macroNoiseScale);
  float detail = fbm(wuv * max(u_detailNoiseScale * 1.8, 0.12));
  float patchNoise = fbm(wuv * max(u_macroNoiseScale * 0.45, 0.006));
  float gravelNoise =
      fbm(wuv * max(u_detailNoiseScale * 3.2, 0.18) + vec2(5.2, 17.3));
  float pebbleNoise = noise21(wuv * max(u_detailNoiseScale * 14.0, 0.9));
  float moistureVar = smoothstep(0.28, 0.72, patchNoise);
  float lush = smoothstep(0.18, 0.82, macro);
  lush = mix(lush, moistureVar, 0.38);
  float lowland = smoothstep(0.0, 0.11, -v_disp);
  float rise = smoothstep(0.03, 0.10, v_disp);
  float windScour = smoothstep(0.45, 0.82, detail + rise * 0.55 + slope * 0.30);

  vec3 lushGrass = mix(u_grassPrimary, u_grassSecondary, lush);
  float dryness = 0.18 + detail * 0.18 + windScour * 0.42;
  dryness += (1.0 - u_moistureLevel) * 0.18;
  dryness -= lowland * (0.18 + 0.24 * u_moistureLevel);
  dryness = clamp(dryness, 0.0, 1.0);
  vec3 grassCol = mix(lushGrass, u_grassDry, dryness);
  grassCol = mix(grassCol, u_grassSecondary, lowland * 0.16);
  float sw = max(0.01, 1.0 / max(u_soilBlendSharpness, 1e-3));
  float sN = (noise21(wuv * 4.0 + 9.7) - 0.5) * sw * 0.85;
  float soilBase = u_soilBlendHeight + lowland * 0.08 - rise * 0.04;
  float soilMix =
      1.0 - smoothstep(soilBase - sw + sN, soilBase + sw + sN, v_worldPos.y);
  soilMix = clamp(soilMix, 0.0, 1.0);
  float mudPatch = fbm(wuv * 0.08 + vec2(7.3, 11.2));
  mudPatch = smoothstep(0.54, 0.72,
                        mudPatch + lowland * 0.20 + u_moistureLevel * 0.10);
  soilMix = max(soilMix, mudPatch * (0.76 + 0.22 * u_moistureLevel));
  soilMix = max(soilMix, lowland * (0.25 + 0.20 * u_moistureLevel));

  vec3 soilCol =
      mix(u_soilColor * 0.96, u_soilColor * 1.05, moistureVar * 0.55);
  vec3 baseCol = mix(grassCol, soilCol, soilMix);

  float gravelMask = smoothstep(0.52, 0.86,
                                gravelNoise * 0.72 + pebbleNoise * 0.28 +
                                    windScour * 0.35 + dryness * 0.30);
  gravelMask *= (0.12 + 0.88 * clamp(u_rockExposure, 0.0, 1.0));
  gravelMask *= 1.0 - lowland * 0.55;
  gravelMask *= 1.0 - soilMix * 0.35;
  vec3 rockCol = mix(u_rockLow, u_rockHigh,
                     clamp(detail * 0.65 + pebbleNoise * 0.35, 0.0, 1.0));
  rockCol *= mix(0.94, 1.08, gravelNoise);
  baseCol = mix(baseCol, rockCol, gravelMask * 0.65);

  if (u_crackIntensity > 0.01) {
    float crackNoise1 = noise21(wuv * 8.0);
    float crackNoise2 = noise21(wuv * 16.0 + vec2(42.0, 17.0));
    float crackPattern = smoothstep(0.45, 0.50, crackNoise1) *
                         smoothstep(0.40, 0.55, crackNoise2);
    crackPattern *= (1.0 - lowland * 0.7) * (0.4 + 0.6 * dryness);
    float crackDarkening = 1.0 - crackPattern * u_crackIntensity * 0.32;
    baseCol *= crackDarkening;
  }

  if (u_snowCoverage > 0.01) {
    float snowNoise = fbm(wuv * 0.5 + vec2(123.0, 456.0));
    float snowAccumulation = smoothstep(0.3, 0.7, snowNoise);
    float heightSnowBonus = smoothstep(-0.5, 1.5, v_worldPos.y) * 0.3;
    float snowMask =
        clamp(snowAccumulation * (u_snowCoverage + heightSnowBonus), 0.0, 1.0);
    vec3 snowTinted = u_snowColor * (1.0 + detail * 0.1);
    baseCol = mix(baseCol, snowTinted, snowMask * 0.85);
  }

  vec3 grayLevel = vec3(dot(baseCol, vec3(0.299, 0.587, 0.114)));
  baseCol = mix(grayLevel, baseCol, u_grassSaturation);

  float puddleMask =
      lowland * (0.15 + 0.85 * u_moistureLevel) * (1.0 - gravelMask * 0.55);
  puddleMask = clamp(puddleMask, 0.0, 1.0);

  float wetDarkening = 1.0 - (u_moistureLevel * 0.12 + puddleMask * 0.10);
  baseCol *= wetDarkening;

  float broadBreakup = fbm(wuv * 0.11 + vec2(31.0, -12.0));
  float dampStain = smoothstep(
      0.55, 0.80, broadBreakup + lowland * 0.16 + u_moistureLevel * 0.12);
  float dryScuff = smoothstep(
      0.64, 0.88, detail * 0.68 + patchNoise * 0.24 + dryness * 0.32);
  vec3 dampSoil =
      mix(u_soilColor * 0.58, u_soilColor * 0.90, moistureVar * 0.65);
  vec3 dustyGrass = mix(u_grassDry, u_soilColor, 0.34);
  float stainWeight =
      dampStain * (0.10 + 0.24 * u_moistureLevel) * (1.0 - gravelMask * 0.45);
  baseCol = mix(baseCol, dampSoil, stainWeight);
  baseCol = mix(baseCol, dustyGrass, dryScuff * 0.13 * (1.0 - soilMix * 0.40));

  vec3 dx = dFdx(v_worldPos);
  float mScale = max(u_detailNoiseScale * (6.0 + u_microBumpFreq * 2.5), 0.2);
  float h0 = fbm(wuv * mScale);
  float hx = fbm((wuv + dFdx(wuv)) * mScale);
  float hy = fbm((wuv + dFdy(wuv)) * mScale);
  vec2 g = vec2(hx - h0, hy - h0);
  vec3 t = normalize(dx - n * dot(n, dx));
  vec3 b = normalize(cross(n, t));
  float reliefMask = clamp(0.35 + gravelMask * 0.55 + soilMix * 0.35 +
                               dryScuff * 0.22 + dampStain * 0.18,
                           0.0, 1.0);
  float microAmp =
      clamp(u_microBumpAmp, 0.02, 0.28) * mix(0.82, 1.55, reliefMask);
  vec3 microPerturb = normalize(n - (t * g.x + b * g.y) * microAmp);
  vec3 nMicro =
      normalize(mix(n, microPerturb, clamp(u_microNormalWeight, 0.0, 1.0)));
  float jitterAmp = max(0.01, u_albedoJitter) * (0.65 + u_soilRoughness * 0.6);
  float jitter = (hash21(wuv * 0.27 + vec2(17.0, 9.0)) - 0.5) * jitterAmp;
  float speckle = step(0.74, noise21(wuv * 23.0 + vec2(2.0, 5.0)));
  float patchBrightness =
      (broadBreakup - 0.5) * 0.11 + (patchNoise - 0.5) * 0.07;
  float brightnessVar =
      (moistureVar - 0.5) * 0.09 + patchBrightness * (1.0 - puddleMask * 0.45);
  vec3 col = baseCol * (1.0 + jitter + brightnessVar);
  col *= 1.0 + (speckle - 0.35) * 0.035 * (1.0 - puddleMask);
  col *= u_tint;
  vec3 L = normalize(u_lightDir);
  float ndl = max(dot(nMicro, L), 0.0);
  float ambient = 0.38 + lowland * 0.04;
  float fres = pow(1.0 - max(dot(nMicro, vec3(0, 1, 0)), 0.0), 2.0);

  float surfaceRoughness =
      mix(0.58, 0.96, clamp(u_soilRoughness + gravelMask * 0.25, 0.0, 1.0));
  surfaceRoughness = mix(surfaceRoughness, 0.34, u_moistureLevel * 0.45);
  surfaceRoughness = mix(surfaceRoughness, 0.18, puddleMask * 0.75);
  float specContrib = fres * 0.10 * (1.0 - surfaceRoughness);
  specContrib += u_moistureLevel * 0.05 * fres;
  specContrib += puddleMask * 0.10 * (0.4 + 0.6 * ndl);
  float shade = ambient + ndl * 0.65 + specContrib;
  vec3 lit = col * shade * (u_ambientBoost + heightTint);

  vec3 dispTint = mix(vec3(0.72, 0.88, 1.02), vec3(1.06, 0.94, 0.82),
                      smoothstep(-0.12, 0.12, v_disp));
  lit = mix(lit, lit * dispTint, 0.14);

  vec3 toCamera = u_cameraPos - v_worldPos;
  float viewDistance = max(length(toCamera), 1e-4);
  vec3 fogViewDir = toCamera / viewDistance;
  float distanceFog =
      smoothstep(u_fogStart, max(u_fogStart + 1e-4, u_fogEnd), viewDistance);
  float horizonFog = smoothstep(0.18, 0.85, 1.0 - abs(fogViewDir.y));
  float fogAmount = clamp(distanceFog * (0.75 + 0.55 * horizonFog), 0.0, 1.0);
  lit = mix(lit, u_fogColor, fogAmount);

  FragColor = vec4(clamp(lit, 0.0, 1.0), 1.0);
}
