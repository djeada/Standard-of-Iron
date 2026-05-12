#version 330 core

in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_uv;
in float v_vertex_displacement;
in float v_entry_mask;

layout(location = 0) out vec4 frag_color;

uniform vec3 u_grass_primary, u_grass_secondary, u_grass_dry, u_soil_color;
uniform vec3 u_rock_low, u_rock_high, u_tint, u_light_dir;
uniform vec2 u_noise_offset;
uniform float u_tile_size, u_macro_noise_scale, u_detail_noise_scale;
uniform float u_slope_rock_threshold, u_slope_rock_sharpness;
uniform float u_soil_blend_height, u_soil_blend_sharpness;
uniform float u_height_noise_strength, u_height_noise_frequency;
uniform float u_ambient_boost, u_rock_detail_strength;

uniform float u_snow_coverage;
uniform float u_moisture_level;
uniform float u_crack_intensity;
uniform float u_rock_exposure;
uniform float u_grass_saturation;
uniform float u_soil_roughness;
uniform float u_curvature_response;
uniform float u_ridge_response;
uniform float u_gully_response;
uniform vec3 u_snow_color;

uniform float u_soil_foot_height;

uniform int u_has_height_tex;
uniform sampler2D u_height_tex;
uniform vec2 u_height_texel_size;
uniform vec2 u_height_uv_scale, u_height_uv_offset;
uniform float u_height_tex_to_world;
uniform int u_toe_tex_radius;
uniform float u_toe_height_delta;
uniform float u_toe_strength;

uniform float u_screen_toe_mul;
uniform float u_screen_toe_clamp;
uniform vec3 u_camera_pos;
uniform vec3 u_fog_color;
uniform float u_fog_start;
uniform float u_fog_end;

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
  float hx = dFdx(v_world_pos.y);
  float hy = dFdy(v_world_pos.y);
  return 0.5 * (dFdx(hx) + dFdy(hy));
}

vec3 geomNormal() {
  vec3 dx = dFdx(v_world_pos);
  vec3 dy = dFdy(v_world_pos);
  vec3 n = normalize(cross(dx, dy));
  return (dot(n, v_normal) < 0.0) ? -n : n;
}

float sampleHeight(vec2 uv) {
  return texture(u_height_tex, uv).r * u_height_tex_to_world;
}

vec2 uvToWorld(vec2 duv) { return duv / max(abs(u_height_uv_scale), vec2(1e-6)); }

vec3 heightmapNormal(vec2 uv) {

  vec2 du = vec2(u_height_texel_size.x, 0.0);
  vec2 dv = vec2(0.0, u_height_texel_size.y);

  float hL = sampleHeight(uv - du);
  float hR = sampleHeight(uv + du);
  float hD = sampleHeight(uv - dv);
  float hU = sampleHeight(uv + dv);

  float dxW = max(1e-6, abs(uvToWorld(du).x));
  float dzW = max(1e-6, abs(uvToWorld(dv).y));

  float dhdx = (hR - hL) / (2.0 * dxW);
  float dhdz = (hU - hD) / (2.0 * dzW);

  return normalize(vec3(-dhdx, 1.0, -dhdz));
}

float avgWorldPerTexel() {
  vec2 wpt = abs(uvToWorld(u_height_texel_size));
  return 0.5 * (wpt.x + wpt.y);
}

float minCliffDistanceRadial(vec2 uv, int r, float riseDelta) {
  const int MAX_R = 12;
  const int NUM_DIR = 12;
  r = clamp(r, 1, MAX_R);

  float h0 = sampleHeight(uv);
  float best = 1e9;

  vec2 texStep = u_height_texel_size;

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
  float entryMask = clamp(v_entry_mask, 0.0, 1.0);
  vec3 normal = geomNormal();

  normal = normalize(mix(normal, normalize(v_normal), entryMask * 0.5));

  if (u_has_height_tex == 1) {
    vec2 huv = v_world_pos.xz * u_height_uv_scale + u_height_uv_offset;
    vec3 hmN = heightmapNormal(huv);
    float slope0 = 1.0 - clamp(normal.y, 0.0, 1.0);

    float w = 0.70 * (1.0 - smoothstep(0.70, 0.95, slope0));

    w *= (1.0 - 0.50 * entryMask);
    normal = normalize(mix(normal, hmN, w));
  }

  float slope = 1.0 - clamp(normal.y, 0.0, 1.0);
  float flatTerrainMask = 1.0 - smoothstep(0.05, 0.18, slope);

  slope *= (1.0 - 0.25 * entryMask);
  float entryShelter = entryMask * (1.0 - smoothstep(0.18, 0.55, slope));
  float entryToe =
      entryMask *
      (1.0 - smoothstep(0.24, 0.62, slope * (1.0 - 0.25 * entryMask)));
  float curvature = computeCurvature();
  float curvatureResponse = clamp(u_curvature_response, 0.0, 1.0);
  float ridgeResponse = clamp(u_ridge_response, 0.0, 1.0);
  float gullyResponse = clamp(u_gully_response, 0.0, 1.0);
  float curvatureGain = mix(1.0, 2.2, curvatureResponse);
  float ridgeThreshold = mix(0.02, 0.008, ridgeResponse);
  float gullyThreshold = mix(0.02, 0.008, gullyResponse);
  float ridgeMask =
      smoothstep(0.0, ridgeThreshold, max(0.0, curvature * curvatureGain));
  float gullyMask =
      smoothstep(0.0, gullyThreshold, max(0.0, -curvature * curvatureGain));

  float tileScale = max(u_tile_size, 0.0001);
  vec2 world_coord = (v_world_pos.xz / tileScale) + u_noise_offset;

  float macroNoise = fbm(world_coord * u_macro_noise_scale);
  float detailNoise =
      triplanarNoise(v_world_pos, u_detail_noise_scale * 2.5 / tileScale);
  float erosionNoise =
      triplanarNoise(v_world_pos, u_detail_noise_scale * 4.0 / tileScale + 17.0);
  float microVariation = fbmDetail(world_coord * u_macro_noise_scale * 8.0);

  float patchNoise = fbm(world_coord * u_macro_noise_scale * 0.4);
  float moistureVar = smoothstep(0.3, 0.7, patchNoise);
  float lushFactor = smoothstep(0.2, 0.8, macroNoise);
  lushFactor = mix(lushFactor, moistureVar, 0.3);
  vec3 lushGrass = mix(u_grass_primary, u_grass_secondary, lushFactor);
  float dryness = clamp(0.55 * slope + 0.45 * detailNoise, 0.0, 1.0);
  dryness = clamp(dryness + ridgeMask * 0.12 * ridgeResponse -
                      gullyMask * 0.10 * gullyResponse,
                  0.0, 1.0);
  dryness =
      clamp(dryness - entryShelter * (0.14 + 0.10 * u_moisture_level), 0.0, 1.0);
  dryness += moistureVar * 0.15;
  dryness =
      mix(dryness, dryness * 0.45 + moistureVar * 0.10, flatTerrainMask * 0.80);

  float heightFade = smoothstep(0.0, 2.5, v_world_pos.y);
  float drynessByHeight = mix(dryness, dryness * 1.15, heightFade * 0.4);
  vec3 grassColor = mix(lushGrass, u_grass_dry, drynessByHeight);
  grassColor *= (1.0 + microVariation * 0.08);
  grassColor = mix(grassColor, mix(u_grass_secondary, u_soil_color, 0.20),
                   entryShelter * (0.10 + 0.10 * u_moisture_level));
  vec3 flatGrassColor =
      mix(u_grass_primary, u_grass_secondary, 0.18 + moistureVar * 0.22);
  grassColor = mix(grassColor, flatGrassColor, flatTerrainMask * 0.45);

  float soilWidth = max(0.01, 1.0 / max(u_soil_blend_sharpness, 0.001));

  float heightNoise =
      (triplanarNoise(v_world_pos, max(0.0001, u_height_noise_frequency)) - 0.5) *
      u_height_noise_strength;
  heightNoise *= mix(1.0, 0.68, flatTerrainMask);

  float toeLocal = smoothstep(0.25, 0.9, slope);

  vec2 dxxz = dFdx(v_world_pos.xz);
  vec2 dyxz = dFdy(v_world_pos.xz);
  float pxWorld = max(length(dxxz), length(dyxz));
  float dh = max(abs(dFdx(v_world_pos.y)), abs(dFdy(v_world_pos.y)));
  float toeSS =
      clamp((dh / max(pxWorld, 1e-6)) * u_screen_toe_mul, 0.0, u_screen_toe_clamp);

  float toeHM = 0.0;
  if (u_has_height_tex == 1) {
    vec2 huv = v_world_pos.xz * u_height_uv_scale + u_height_uv_offset;
    float dW = minCliffDistanceRadial(huv, u_toe_tex_radius,
                                      max(1e-4, u_toe_height_delta));
    float maxSearchW = avgWorldPerTexel() * float(u_toe_tex_radius);
    if (dW < 1e8) {
      toeHM = smoothstep(maxSearchW, 0.0, dW) * clamp(u_toe_strength, 0.0, 1.0);
    }
  }

  float toeProximity =
      max(toeLocal, max(toeHM, toeSS / max(1e-6, u_soil_foot_height)));

  float concavityLift =
      gullyMask * ((0.25 + 0.25 * gullyResponse) * u_soil_foot_height);

  float soilHeight = u_soil_blend_height + heightNoise + concavityLift +
                     entryToe * u_soil_foot_height * 0.45;
  float bandWidth =
      soilWidth + u_soil_foot_height * max(toeProximity, entryToe * 0.85);

  float soilMix = 1.0 - smoothstep(soilHeight - bandWidth,
                                   soilHeight + bandWidth, v_world_pos.y);
  soilMix = clamp(soilMix, 0.0, 1.0);
  soilMix = max(soilMix, entryShelter * (0.10 + 0.18 * (1.0 - slope)));

  float mudPatch = fbm(world_coord * 0.08 + vec2(7.3, 11.2));
  mudPatch = smoothstep(0.57, 0.76,
                        mudPatch + gullyMask * 0.10 + u_moisture_level * 0.08);
  mudPatch *= (1.0 - flatTerrainMask * 0.18);
  soilMix = max(soilMix, mudPatch * 0.88 * (1.0 - slope * 0.55));
  soilMix *= (1.0 - flatTerrainMask * (0.34 + 0.06 * u_moisture_level));

  vec3 soilBlend = mix(grassColor, u_soil_color, soilMix);

  float slopeCut =
      smoothstep(u_slope_rock_threshold, u_slope_rock_threshold + 0.02, slope);
  float rockMask = clamp(pow(slopeCut, max(1.0, u_slope_rock_sharpness)) +
                             (erosionNoise - 0.5) * u_rock_detail_strength,
                         0.0, 1.0);
  rockMask *= 1.0 - soilMix * 0.75;

  rockMask *= (1.0 - 0.50 * entryShelter);
  rockMask *= (1.0 - flatTerrainMask * 0.85);

  float rockLerp = clamp(0.35 + detailNoise * 0.65, 0.0, 1.0);
  vec3 rockColor = mix(u_rock_low, u_rock_high, rockLerp);

  float rockDetailVariation = fbmDetail(world_coord * 0.15) * 0.5 + 0.5;
  rockColor *= mix(0.92, 1.08, rockDetailVariation);
  rockColor = mix(rockColor, rockColor * 1.12,
                  clamp(u_rock_detail_strength * 1.3, 0.0, 1.0));

  vec3 microNormal = normal;
  float microDetailScale = u_detail_noise_scale * 8.0 / tileScale;
  vec2 microOffset = vec2(0.008, 0.0);
  float h0 = triplanarNoise(v_world_pos, microDetailScale);
  float hx = triplanarNoise(v_world_pos + vec3(microOffset.x, 0.0, 0.0),
                            microDetailScale);
  float hz = triplanarNoise(v_world_pos + vec3(0.0, 0.0, microOffset.x),
                            microDetailScale);
  vec3 microGrad =
      vec3((hx - h0) / microOffset.x, 0.0, (hz - h0) / microOffset.x);
  float microAmp = 0.18 * u_rock_detail_strength * (0.15 + 0.85 * slope);
  microAmp *= mix(1.0, 0.72, flatTerrainMask);
  microNormal = normalize(normal + microGrad * microAmp);

  float fineDetail = triplanarNoise(v_world_pos, microDetailScale * 2.5);
  vec3 fineNormalPerturb =
      vec3((fineDetail - 0.5) * 0.03, 0.0,
           (triplanarNoise(v_world_pos + vec3(0.1, 0.0, 0.0),
                           microDetailScale * 2.5) -
            0.5) *
               0.03);
  microNormal =
      normalize(microNormal + fineNormalPerturb * (0.3 + 0.7 * rockMask));

  float isFlat = 1.0 - smoothstep(0.10, 0.25, slope);
  float isHigh = smoothstep(u_soil_blend_height + 0.5, u_soil_blend_height + 1.5,
                            v_world_pos.y);
  float plateauFactor = isFlat * isHigh;
  float isGully = gullyMask * (1.0 - smoothstep(0.25, 0.6, slope));
  float isSteep = smoothstep(0.3, 0.5, slope);
  float isRim = ridgeMask;
  float rimFactor = isSteep * isRim;

  rockMask =
      clamp(rockMask + rimFactor * (0.10 + 0.16 * ridgeResponse) -
                plateauFactor * 0.06 - isGully * (0.08 + 0.12 * gullyResponse),
            0.0, 1.0);

  rockMask = clamp(rockMask + (u_rock_exposure - 0.3) * 0.4, 0.0, 1.0);

  vec3 terrainColor = mix(soilBlend, rockColor, rockMask);
  terrainColor = mix(terrainColor, soilBlend, entryShelter * 0.12);

  if (u_crack_intensity > 0.01) {
    float crackNoise1 = noise21(world_coord * 8.0);
    float crackNoise2 = noise21(world_coord * 16.0 + vec2(42.0, 17.0));
    float crackPattern = smoothstep(0.45, 0.50, crackNoise1) *
                         smoothstep(0.40, 0.55, crackNoise2);
    crackPattern *= (1.0 - slope * 0.8);
    float crackDarkening = 1.0 - crackPattern * u_crack_intensity * 0.35;
    terrainColor *= crackDarkening;
  }

  if (u_snow_coverage > 0.01) {
    float snowNoise = fbm(world_coord * 0.5 + vec2(123.0, 456.0));
    float snowAccumulation = smoothstep(0.3, 0.7, snowNoise);

    float slopeSnowReduction = 1.0 - smoothstep(0.15, 0.45, slope);

    float heightSnowBonus = smoothstep(-0.5, 1.5, v_world_pos.y) * 0.3;
    float snowMask = clamp(snowAccumulation * slopeSnowReduction *
                               (u_snow_coverage + heightSnowBonus),
                           0.0, 1.0);

    vec3 snowTinted = u_snow_color * (1.0 + detailNoise * 0.1);
    terrainColor = mix(terrainColor, snowTinted, snowMask * 0.85);
  }

  vec3 grayLevel = vec3(dot(terrainColor, vec3(0.299, 0.587, 0.114)));
  terrainColor = mix(grayLevel, terrainColor, u_grass_saturation);

  float moistureEffect = u_moisture_level;

  float wetDarkening = 1.0 - moistureEffect * 0.15 * (1.0 - rockMask);
  terrainColor *= wetDarkening;

  float jitterAmp = 0.06 * (0.5 + u_soil_roughness * 0.5);
  jitterAmp *= (1.0 - 0.16 * flatTerrainMask);
  float jitter =
      (hash21(world_coord * 0.27 + vec2(17.0, 9.0)) - 0.5) * jitterAmp;
  float brightnessVar = (moistureVar - 0.5) * 0.08 * (1.0 - rockMask);
  terrainColor *= (1.0 + jitter + brightnessVar) * u_tint;

  vec3 L = normalize(u_light_dir);
  float ndl = max(dot(microNormal, L), 0.0);

  float skyOcclusion =
      max(smoothstep(-0.03, 0.01, -curvature), gullyMask * gullyResponse);
  float ao =
      mix(1.0, 0.75 - 0.08 * gullyResponse, skyOcclusion * (1.0 - slope * 0.5));

  float ambient = 0.32 * ao;
  float fresnel =
      pow(1.0 - max(dot(microNormal, vec3(0.0, 1.0, 0.0)), 0.0), 2.2);

  float surfaceRoughness = mix(0.65, 0.95, u_soil_roughness);
  surfaceRoughness = mix(surfaceRoughness, 0.42, u_moisture_level * 0.5);

  vec3 viewDir = normalize(vec3(0.0, 1.0, -0.5));
  vec3 halfDir = normalize(L + viewDir);
  float specAngle = max(dot(microNormal, halfDir), 0.0);
  float specular = pow(specAngle, mix(4.0, 32.0, 1.0 - surfaceRoughness));

  float specContrib =
      fresnel * 0.14 * (1.0 - surfaceRoughness) * (1.0 - rockMask) * specular;

  specContrib += u_moisture_level * 0.10 * fresnel * (1.0 - rockMask);
  float shade = ambient + ndl * 0.78 + specContrib;

  float plateauMuted = plateauFactor * (1.0 - 0.70 * entryMask);
  float plateauDesat = clamp(0.75 * plateauMuted, 0.0, 0.75);
  float plateauDim = clamp(0.25 * plateauMuted, 0.0, 0.25);

  float lumaP = dot(terrainColor, vec3(0.299, 0.587, 0.114));
  terrainColor = mix(terrainColor, vec3(lumaP), plateauDesat);

  float plateauBrightness = 1.0 - plateauDim;
  float gullyDarkness = 1.0 - isGully * (0.04 + 0.07 * gullyResponse);
  float rimContrast = 1.0 + rimFactor * (0.03 + 0.05 * ridgeResponse);

  {
    float trackStrength =
        smoothstep(0.2, 0.7, u_moisture_level) * (1.0 - rockMask * 0.9);
    float track1 = step(0.76, noise21(world_coord * 2.8 + vec2(3.17, 7.53)));
    float track2 = step(0.76, noise21(world_coord * 2.8 + vec2(11.4, 2.81)));
    float track3 = step(0.80, noise21(world_coord * 5.5 + vec2(19.2, 5.6)));
    float trackMask = max(max(track1, track2), track3) * trackStrength;
    vec3 mudColor = mix(terrainColor * 0.68, vec3(0.30, 0.22, 0.14), 0.35);
    terrainColor = mix(terrainColor, mudColor, clamp(trackMask, 0.0, 0.55));
  }

  terrainColor *= plateauBrightness * gullyDarkness * rimContrast;
  vec3 litColor = terrainColor * shade * u_ambient_boost;

  vec3 sun_tint = vec3(1.08, 0.94, 0.80);
  vec3 shadow_tint = vec3(0.78, 0.86, 1.04);
  float grade_t = clamp(ndl * 1.4, 0.0, 1.0);
  litColor *= mix(shadow_tint, sun_tint, grade_t);

  vec3 toCamera = u_camera_pos - v_world_pos;
  float viewDistance = max(length(toCamera), 1e-4);
  vec3 fogViewDir = toCamera / viewDistance;
  float distanceFog =
      smoothstep(u_fog_start, max(u_fog_start + 1e-4, u_fog_end), viewDistance);
  float horizonFog = smoothstep(0.20, 0.88, 1.0 - abs(fogViewDir.y));
  float fogAmount = clamp(distanceFog * (0.72 + 0.60 * horizonFog), 0.0, 1.0);
  litColor = mix(litColor, u_fog_color, fogAmount);

  frag_color = vec4(clamp(litColor, 0.0, 1.0), 1.0);
}
