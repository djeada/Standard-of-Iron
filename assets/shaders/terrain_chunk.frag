#version 330 core

in vec3 v_worldPos;
in vec3 v_normal;
in vec2 v_uv;
in float v_vertexDisplacement;

layout(location = 0) out vec4 FragColor;

uniform vec3 u_grassPrimary, u_grassSecondary, u_grassDry, u_soilColor;
uniform vec3 u_rockLow, u_rockHigh, u_tint, u_lightDir;
uniform vec2 u_noiseOffset;
uniform float u_tileSize, u_macroNoiseScale, u_detailNoiseScale;
uniform float u_slopeRockThreshold, u_slopeRockSharpness;
uniform float u_soilBlendHeight, u_soilBlendSharpness;
uniform float u_heightNoiseStrength, u_heightNoiseFrequency;
uniform float u_ambientBoost, u_rockDetailStrength;

uniform float u_snowCoverage;
uniform float u_moistureLevel;
uniform float u_crackIntensity;
uniform float u_rockExposure;
uniform float u_grassSaturation;
uniform float u_soilRoughness;
uniform vec3 u_snowColor;

uniform float u_soilFootHeight;

uniform int u_hasHeightTex;
uniform sampler2D u_heightTex;
uniform vec2 u_heightTexelSize;
uniform vec2 u_heightUVScale, u_heightUVOffset;
uniform float u_heightTexToWorld;
uniform int u_toeTexRadius;
uniform float u_toeHeightDelta;
uniform float u_toeStrength;

uniform float u_screenToeMul;
uniform float u_screenToeClamp;

float hash21(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise21(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  float a = hash21(i);
  float b = hash21(i + vec2(1.0, 0.0));
  float c = hash21(i + vec2(0.0, 1.0));
  float d = hash21(i + vec2(1.0, 1.0));
  vec2 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(vec2 p) {
  float v = 0.0, a = 0.5;
  for (int i = 0; i < 5; ++i) {
    v += noise21(p) * a;
    p = p * 2.07 + 13.17;
    a *= 0.5;
  }
  return v;
}

float fbmDetail(vec2 p) {
  float v = 0.0, a = 0.5;
  for (int i = 0; i < 6; ++i) {
    v += noise21(p) * a;
    p = p * 2.13 + 7.89;
    a *= 0.52;
  }
  return v;
}

vec3 triplanarWeights(vec3 n) {
  vec3 b = abs(n);
  b = pow(b, vec3(4.0));
  return b / (b.x + b.y + b.z + 1e-5);
}

float triplanarNoise(vec3 wp, float s) {
  vec3 w = triplanarWeights(normalize(v_normal));
  float xy = noise21(wp.xy * s);
  float xz = noise21(wp.xz * s);
  float yz = noise21(wp.yz * s);
  return xy * w.z + xz * w.y + yz * w.x;
}

float computeCurvature() {
  float hx = dFdx(v_worldPos.y);
  float hy = dFdy(v_worldPos.y);
  return 0.5 * (dFdx(hx) + dFdy(hy));
}

vec3 geomNormal() {
  vec3 dx = dFdx(v_worldPos);
  vec3 dy = dFdy(v_worldPos);
  vec3 n = normalize(cross(dx, dy));
  return (dot(n, v_normal) < 0.0) ? -n : n;
}

float sampleHeight(vec2 uv) {
  return texture(u_heightTex, uv).r * u_heightTexToWorld;
}

vec2 uvToWorld(vec2 duv) { return duv / max(abs(u_heightUVScale), vec2(1e-6)); }

float avgWorldPerTexel() {
  vec2 wpt = abs(uvToWorld(u_heightTexelSize));
  return 0.5 * (wpt.x + wpt.y);
}

float minCliffDistanceRadial(vec2 uv, int r, float riseDelta) {
  const int MAX_R = 12;
  const int NUM_DIR = 12;
  r = clamp(r, 1, MAX_R);

  float h0 = sampleHeight(uv);
  float best = 1e9;

  vec2 texStep = u_heightTexelSize;

  for (int d = 0; d < NUM_DIR; ++d) {
    float ang = 6.2831853 * (float(d) + 0.5) / float(NUM_DIR);
    vec2 dir = normalize(vec2(cos(ang), sin(ang))) * texStep;

    vec2 p = uv;
    for (int s = 1; s <= MAX_R; ++s) {
      if (s > r)
        break;
      p += dir;

      float rise = sampleHeight(p) - h0;
      if (rise > riseDelta) {
        float stepWorld = length(uvToWorld(dir));
        float distWorld = stepWorld * float(s);
        best = min(best, distWorld);
        break;
      }
    }
  }
  return best;
}

void main() {
  vec3 normal = geomNormal();
  float slope = 1.0 - clamp(normal.y, 0.0, 1.0);
  float curvature = computeCurvature();

  float tileScale = max(u_tileSize, 0.0001);
  vec2 world_coord = (v_worldPos.xz / tileScale) + u_noiseOffset;

  float macroNoise = fbm(world_coord * u_macroNoiseScale);
  float detailNoise =
      triplanarNoise(v_worldPos, u_detailNoiseScale * 2.5 / tileScale);
  float erosionNoise =
      triplanarNoise(v_worldPos, u_detailNoiseScale * 4.0 / tileScale + 17.0);
  float microVariation = fbmDetail(world_coord * u_macroNoiseScale * 8.0);

  float patchNoise = fbm(world_coord * u_macroNoiseScale * 0.4);
  float moistureVar = smoothstep(0.3, 0.7, patchNoise);
  float lushFactor = smoothstep(0.2, 0.8, macroNoise);
  lushFactor = mix(lushFactor, moistureVar, 0.3);
  vec3 lushGrass = mix(u_grassPrimary, u_grassSecondary, lushFactor);
  float dryness = clamp(0.55 * slope + 0.45 * detailNoise, 0.0, 1.0);
  dryness += moistureVar * 0.15;
  
  float heightFade = smoothstep(0.0, 2.5, v_worldPos.y);
  float drynessByHeight = mix(dryness, dryness * 1.15, heightFade * 0.4);
  vec3 grassColor = mix(lushGrass, u_grassDry, drynessByHeight);
  grassColor *= (1.0 + microVariation * 0.08);

  float soilWidth = max(0.01, 1.0 / max(u_soilBlendSharpness, 0.001));

  float heightNoise =
      (triplanarNoise(v_worldPos, max(0.0001, u_heightNoiseFrequency)) - 0.5) *
      u_heightNoiseStrength;

  float toeLocal = smoothstep(0.25, 0.9, slope);

  vec2 dxxz = dFdx(v_worldPos.xz);
  vec2 dyxz = dFdy(v_worldPos.xz);
  float pxWorld = max(length(dxxz), length(dyxz));
  float dh = max(abs(dFdx(v_worldPos.y)), abs(dFdy(v_worldPos.y)));
  float toeSS =
      clamp((dh / max(pxWorld, 1e-6)) * u_screenToeMul, 0.0, u_screenToeClamp);

  float toeHM = 0.0;
  if (u_hasHeightTex == 1) {
    vec2 huv = v_worldPos.xz * u_heightUVScale + u_heightUVOffset;
    float dW = minCliffDistanceRadial(huv, u_toeTexRadius,
                                      max(1e-4, u_toeHeightDelta));
    float maxSearchW = avgWorldPerTexel() * float(u_toeTexRadius);
    if (dW < 1e8) {
      toeHM = smoothstep(maxSearchW, 0.0, dW) * clamp(u_toeStrength, 0.0, 1.0);
    }
  }

  float toeProximity =
      max(toeLocal, max(toeHM, toeSS / max(1e-6, u_soilFootHeight)));

  float concavityLift =
      smoothstep(0.0, 0.02, -curvature) * (0.25 * u_soilFootHeight);

  float soilHeight = u_soilBlendHeight + heightNoise + concavityLift;
  float bandWidth = soilWidth + u_soilFootHeight * toeProximity;

  float soilMix = 1.0 - smoothstep(soilHeight - bandWidth,
                                   soilHeight + bandWidth, v_worldPos.y);
  soilMix = clamp(soilMix, 0.0, 1.0);

  float mudPatch = fbm(world_coord * 0.08 + vec2(7.3, 11.2));
  mudPatch = smoothstep(0.65, 0.75, mudPatch);
  soilMix = max(soilMix, mudPatch * 0.85 * (1.0 - slope * 0.6));

  vec3 soilBlend = mix(grassColor, u_soilColor, soilMix);

  float slopeCut =
      smoothstep(u_slopeRockThreshold, u_slopeRockThreshold + 0.02, slope);
  float rockMask = clamp(pow(slopeCut, max(1.0, u_slopeRockSharpness)) +
                             (erosionNoise - 0.5) * u_rockDetailStrength,
                         0.0, 1.0);
  rockMask *= 1.0 - soilMix * 0.75;

  float rockLerp = clamp(0.35 + detailNoise * 0.65, 0.0, 1.0);
  vec3 rockColor = mix(u_rockLow, u_rockHigh, rockLerp);
  
  float rockDetailVariation = fbmDetail(world_coord * 0.15) * 0.5 + 0.5;
  rockColor *= mix(0.92, 1.08, rockDetailVariation);
  rockColor = mix(rockColor, rockColor * 1.12,
                  clamp(u_rockDetailStrength * 1.3, 0.0, 1.0));

  vec3 microNormal = normal;
  float microDetailScale = u_detailNoiseScale * 8.0 / tileScale;
  vec2 microOffset = vec2(0.008, 0.0);
  float h0 = triplanarNoise(v_worldPos, microDetailScale);
  float hx = triplanarNoise(v_worldPos + vec3(microOffset.x, 0.0, 0.0),
                            microDetailScale);
  float hz = triplanarNoise(v_worldPos + vec3(0.0, 0.0, microOffset.x),
                            microDetailScale);
  vec3 microGrad =
      vec3((hx - h0) / microOffset.x, 0.0, (hz - h0) / microOffset.x);
  float microAmp = 0.18 * u_rockDetailStrength * (0.15 + 0.85 * slope);
  microNormal = normalize(normal + microGrad * microAmp);
  
  float fineDetail = triplanarNoise(v_worldPos, microDetailScale * 2.5);
  vec3 fineNormalPerturb = vec3(
    (fineDetail - 0.5) * 0.03,
    0.0,
    (triplanarNoise(v_worldPos + vec3(0.1, 0.0, 0.0), microDetailScale * 2.5) - 0.5) * 0.03
  );
  microNormal = normalize(microNormal + fineNormalPerturb * (0.3 + 0.7 * rockMask));

  float isFlat = 1.0 - smoothstep(0.10, 0.25, slope);
  float isHigh = smoothstep(u_soilBlendHeight + 0.5, u_soilBlendHeight + 1.5,
                            v_worldPos.y);
  float plateauFactor = isFlat * isHigh;
  float isGully =
      smoothstep(0.0, 0.02, -curvature) * (1.0 - smoothstep(0.25, 0.6, slope));
  float isSteep = smoothstep(0.3, 0.5, slope);
  float isRim = smoothstep(0.0, 0.02, curvature);
  float rimFactor = isSteep * isRim;

  rockMask =
      clamp(rockMask + rimFactor * 0.10 - plateauFactor * 0.06 - isGully * 0.08,
            0.0, 1.0);

  rockMask = clamp(rockMask + (u_rockExposure - 0.3) * 0.4, 0.0, 1.0);

  vec3 terrainColor = mix(soilBlend, rockColor, rockMask);

  if (u_crackIntensity > 0.01) {
    float crackNoise1 = noise21(world_coord * 8.0);
    float crackNoise2 = noise21(world_coord * 16.0 + vec2(42.0, 17.0));
    float crackPattern = smoothstep(0.45, 0.50, crackNoise1) *
                         smoothstep(0.40, 0.55, crackNoise2);
    crackPattern *= (1.0 - slope * 0.8);
    float crackDarkening = 1.0 - crackPattern * u_crackIntensity * 0.35;
    terrainColor *= crackDarkening;
  }

  if (u_snowCoverage > 0.01) {
    float snowNoise = fbm(world_coord * 0.5 + vec2(123.0, 456.0));
    float snowAccumulation = smoothstep(0.3, 0.7, snowNoise);

    float slopeSnowReduction = 1.0 - smoothstep(0.15, 0.45, slope);

    float heightSnowBonus = smoothstep(-0.5, 1.5, v_worldPos.y) * 0.3;
    float snowMask = clamp(snowAccumulation * slopeSnowReduction *
                               (u_snowCoverage + heightSnowBonus),
                           0.0, 1.0);

    vec3 snowTinted = u_snowColor * (1.0 + detailNoise * 0.1);
    terrainColor = mix(terrainColor, snowTinted, snowMask * 0.85);
  }

  vec3 grayLevel = vec3(dot(terrainColor, vec3(0.299, 0.587, 0.114)));
  terrainColor = mix(grayLevel, terrainColor, u_grassSaturation);

  float moistureEffect = u_moistureLevel;

  float wetDarkening = 1.0 - moistureEffect * 0.15 * (1.0 - rockMask);
  terrainColor *= wetDarkening;

  float jitterAmp = 0.06 * (0.5 + u_soilRoughness * 0.5);
  float jitter =
      (hash21(world_coord * 0.27 + vec2(17.0, 9.0)) - 0.5) * jitterAmp;
  float brightnessVar = (moistureVar - 0.5) * 0.08 * (1.0 - rockMask);
  terrainColor *= (1.0 + jitter + brightnessVar) * u_tint;

  vec3 L = normalize(u_lightDir);
  float ndl = max(dot(microNormal, L), 0.0);
  
  float skyOcclusion = smoothstep(-0.03, 0.01, -curvature);
  float ao = mix(1.0, 0.75, skyOcclusion * (1.0 - slope * 0.5));
  
  float ambient = 0.32 * ao;
  float fresnel =
      pow(1.0 - max(dot(microNormal, vec3(0.0, 1.0, 0.0)), 0.0), 2.2);

  float surfaceRoughness = mix(0.65, 0.95, u_soilRoughness);
  surfaceRoughness = mix(surfaceRoughness, 0.42, u_moistureLevel * 0.5);
  
  vec3 viewDir = normalize(vec3(0.0, 1.0, -0.5));
  vec3 halfDir = normalize(L + viewDir);
  float specAngle = max(dot(microNormal, halfDir), 0.0);
  float specular = pow(specAngle, mix(4.0, 32.0, 1.0 - surfaceRoughness));
  
  float specContrib =
      fresnel * 0.14 * (1.0 - surfaceRoughness) * (1.0 - rockMask) * specular;

  specContrib += u_moistureLevel * 0.10 * fresnel * (1.0 - rockMask);
  float shade = ambient + ndl * 0.78 + specContrib;

  float plateauBrightness = 1.0 + plateauFactor * 0.05;
  float gullyDarkness = 1.0 - isGully * 0.04;
  float rimContrast = 1.0 + rimFactor * 0.03;

  terrainColor *= plateauBrightness * gullyDarkness * rimContrast;
  vec3 litColor = terrainColor * shade * u_ambientBoost;

  FragColor = vec4(clamp(litColor, 0.0, 1.0), 1.0);
}
