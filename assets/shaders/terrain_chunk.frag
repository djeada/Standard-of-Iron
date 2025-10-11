#version 330 core

in vec3 v_worldPos;
in vec3 v_normal;
in vec2 v_uv;
in float v_vertexDisplacement;

layout (location = 0) out vec4 FragColor;

uniform vec3  u_grassPrimary, u_grassSecondary, u_grassDry, u_soilColor;
uniform vec3  u_rockLow, u_rockHigh, u_tint, u_lightDir;
uniform vec2  u_noiseOffset;
uniform float u_tileSize, u_macroNoiseScale, u_detailNoiseScale;
uniform float u_slopeRockThreshold, u_slopeRockSharpness;
uniform float u_soilBlendHeight, u_soilBlendSharpness;
uniform float u_heightNoiseStrength, u_heightNoiseFrequency;
uniform float u_ambientBoost, u_rockDetailStrength;

// lets soil “climb” up steep toes (world units)
uniform float u_soilFootHeight;          // try 0.6–1.2

// -------- OPTIONAL (leave at defaults if you don’t have a heightmap) --------
uniform int   u_hasHeightTex;            // 0 = off (default), 1 = on
uniform sampler2D u_heightTex;
uniform vec2  u_heightTexelSize;
uniform vec2  u_heightUVScale, u_heightUVOffset;
uniform float u_heightTexToWorld;        // height normalization -> world units
uniform int   u_toeTexRadius;            // 3–6
uniform float u_toeHeightDelta;          // ~0.5–2.0 world units
uniform float u_toeStrength;             // 0..1
// ----------------------------------------------------------------------------

// NEW: view-consistent, data-free toe smoothing (works even without heightmap)
uniform float u_screenToeMul;            // try 12.0–30.0
uniform float u_screenToeClamp;          // max extra width in world units (try 0.8)

float hash21(vec2 p){ return fract(sin(dot(p, vec2(127.1,311.7))) * 43758.5453123); }

float noise21(vec2 p){
    vec2 i = floor(p), f = fract(p);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0,0.0));
    float c = hash21(i + vec2(0.0,1.0));
    float d = hash21(i + vec2(1.0,1.0));
    vec2 u = f*f*(3.0 - 2.0*f);
    return mix(mix(a,b,u.x), mix(c,d,u.x), u.y);
}

float fbm(vec2 p){
    float v=0.0, a=0.5;
    for(int i=0;i<4;++i){ v += noise21(p)*a; p = p*2.07+13.17; a*=0.5; }
    return v;
}

vec3 triplanarWeights(vec3 n){
    vec3 b = abs(n); b = pow(b, vec3(4.0));
    return b / (b.x + b.y + b.z + 1e-5);
}

float triplanarNoise(vec3 wp, float s){
    vec3 w = triplanarWeights(normalize(v_normal));
    float xy = noise21(wp.xy * s);
    float xz = noise21(wp.xz * s);
    float yz = noise21(wp.yz * s);
    return xy*w.z + xz*w.y + yz*w.x;
}

float computeCurvature(){
    float hx = dFdx(v_worldPos.y);
    float hy = dFdy(v_worldPos.y);
    return 0.5 * (dFdx(hx) + dFdy(hy));
}

vec3 geomNormal(){
    vec3 dx = dFdx(v_worldPos);
    vec3 dy = dFdy(v_worldPos);
    vec3 n  = normalize(cross(dx,dy));
    return (dot(n, v_normal) < 0.0) ? -n : n;
}

// ---- Optional heightmap helpers --------------------------------------------
float sampleHeight(vec2 uv){ return texture(u_heightTex, uv).r * u_heightTexToWorld; }

// (kept for reference; no longer used by the fix)
// float maxRiseNearby(vec2 uv, int r){ ... }  // removed to avoid confusion

// --- world <-> UV helpers for height texture sampling (FIX ADD) ---
vec2 uvToWorld(vec2 duv){
    // uv = worldXZ * u_heightUVScale + u_heightUVOffset
    // => worldXZ delta per uv delta = duv / u_heightUVScale (component-wise)
    return duv / max(abs(u_heightUVScale), vec2(1e-6));
}

float avgWorldPerTexel(){
    vec2 wpt = abs(uvToWorld(u_heightTexelSize));
    return 0.5 * (wpt.x + wpt.y);
}

// Radial min-distance (in WORLD units) to a higher “cliff” neighborhood (FIX ADD)
float minCliffDistanceRadial(vec2 uv, int r, float riseDelta){
    const int MAX_R   = 12;      // max steps per ray (in texels)
    const int NUM_DIR = 12;      // number of ray directions
    r = clamp(r, 1, MAX_R);

    float h0 = sampleHeight(uv);
    float best = 1e9;

    vec2 texStep = u_heightTexelSize;

    for(int d = 0; d < NUM_DIR; ++d){
        float ang = 6.2831853 * (float(d) + 0.5) / float(NUM_DIR);
        vec2 dir = normalize(vec2(cos(ang), sin(ang))) * texStep;

        vec2 p = uv;
        for(int s = 1; s <= MAX_R; ++s){
            if(s > r) break;
            p += dir;

            float rise = sampleHeight(p) - h0;
            if(rise > riseDelta){
                float stepWorld = length(uvToWorld(dir));
                float distWorld = stepWorld * float(s);
                best = min(best, distWorld);
                break;
            }
        }
    }
    return best; // 1e9 = not found
}
// ----------------------------------------------------------------------------

void main(){
    vec3 normal = geomNormal();
    float slope = 1.0 - clamp(normal.y, 0.0, 1.0);
    float curvature = computeCurvature();

    float tileScale = max(u_tileSize, 0.0001);
    vec2 worldCoord = (v_worldPos.xz / tileScale) + u_noiseOffset;

    float macroNoise   = fbm(worldCoord * u_macroNoiseScale);
    float detailNoise  = triplanarNoise(v_worldPos, u_detailNoiseScale * 2.5 / tileScale);
    float erosionNoise = triplanarNoise(v_worldPos, u_detailNoiseScale * 4.0 / tileScale + 17.0);

    // Large-scale patch noise for bare ground/mud patches
    float patchNoise = noise21(worldCoord * u_macroNoiseScale * 3.5 + vec2(7.3, 2.1));
    float mudPatch = smoothstep(0.50, 0.75, patchNoise);
    
    // Moisture variation (large-scale darkening/lightening)
    float moisture = fbm(worldCoord * u_macroNoiseScale * 2.2 + vec2(5.7, 9.3));

    float lushFactor = smoothstep(0.2, 0.8, macroNoise);
    vec3  lushGrass  = mix(u_grassPrimary, u_grassSecondary, lushFactor);
    float dryness    = clamp(0.55 * slope + 0.45 * detailNoise, 0.0, 1.0);
    vec3  grassColor = mix(lushGrass, u_grassDry, dryness);
    
    // Apply moisture-based brightness variation (very dramatic for visibility)
    float moistureFactor = moisture * 0.50 - 0.25;
    grassColor *= (1.0 + moistureFactor);
    
    // Define mud color (much darker for visibility)
    vec3 mudColor = u_soilColor * 0.50;
    
    // Mix in mud patches (independent of height, but reduced on slopes) - very strong
    grassColor = mix(grassColor, mudColor, mudPatch * 0.85 * (1.0 - slope * 0.5));

    // ----- Soil band (height + toe expansion) -----
    float soilWidth = max(0.01, 1.0 / max(u_soilBlendSharpness, 0.001));

    // gentle noise to avoid tiled edges
    float heightNoise = (triplanarNoise(v_worldPos, max(0.0001, u_heightNoiseFrequency)) - 0.5)
                        * u_heightNoiseStrength;

    // local toe from slope (small on flats)
    float toeLocal = smoothstep(0.25, 0.9, slope);

    // world-consistent “screen-space” dilation (FIX REPLACE)
    vec2 dxxz = dFdx(v_worldPos.xz);
    vec2 dyxz = dFdy(v_worldPos.xz);
    float pxWorld = max(length(dxxz), length(dyxz)); // world meters per pixel (approx)
    float dh = max(abs(dFdx(v_worldPos.y)), abs(dFdy(v_worldPos.y))); // height change per pixel
    float toeSS = clamp((dh / max(pxWorld, 1e-6)) * u_screenToeMul, 0.0, u_screenToeClamp);

    // heightmap-based toe: isotropic distance-to-cliff (FIX REPLACE)
    float toeHM = 0.0;
    if(u_hasHeightTex == 1){
        vec2 huv = v_worldPos.xz * u_heightUVScale + u_heightUVOffset;
        float dW = minCliffDistanceRadial(huv, u_toeTexRadius, max(1e-4, u_toeHeightDelta));
        float maxSearchW = avgWorldPerTexel() * float(u_toeTexRadius);
        if(dW < 1e8){
            toeHM = smoothstep(maxSearchW, 0.0, dW) * clamp(u_toeStrength, 0.0, 1.0);
        }
    }

    float toeProximity = max(toeLocal, max(toeHM, toeSS / max(1e-6, u_soilFootHeight)));

    // concave bias
    float concavityLift = smoothstep(0.0, 0.02, -curvature) * (0.25 * u_soilFootHeight);

    float soilHeight = u_soilBlendHeight + heightNoise + concavityLift;
    float bandWidth  = soilWidth + u_soilFootHeight * toeProximity;

    float soilMix = 1.0 - smoothstep(soilHeight - bandWidth,
                                     soilHeight + bandWidth,
                                     v_worldPos.y);
    soilMix = clamp(soilMix, 0.0, 1.0);
    vec3 soilBlend = mix(grassColor, u_soilColor, soilMix);

    // ----- Rocks -----
    float slopeCut = smoothstep(u_slopeRockThreshold, u_slopeRockThreshold + 0.02, slope);
    float rockMask = clamp(
        pow(slopeCut, max(1.0, u_slopeRockSharpness)) +
        (erosionNoise - 0.5) * u_rockDetailStrength,
        0.0, 1.0
    );
    rockMask *= 1.0 - soilMix * 0.75;

    float rockLerp = clamp(0.35 + detailNoise * 0.65, 0.0, 1.0);
    vec3 rockColor = mix(u_rockLow, u_rockHigh, rockLerp);
    rockColor = mix(rockColor, rockColor * 1.15, clamp(u_rockDetailStrength * 1.4, 0.0, 1.0));

    // micro normal
    vec3 microNormal = normal;
    float microDetailScale = u_detailNoiseScale * 8.0 / tileScale;
    vec2 microOffset = vec2(0.01, 0.0);
    float h0 = triplanarNoise(v_worldPos, microDetailScale);
    float hx = triplanarNoise(v_worldPos + vec3(microOffset.x, 0.0, 0.0), microDetailScale);
    float hz = triplanarNoise(v_worldPos + vec3(0.0, 0.0, microOffset.x), microDetailScale);
    vec3 microGrad = vec3((hx - h0) / microOffset.x, 0.0, (hz - h0) / microOffset.x);
    float microAmp = 0.15 * u_rockDetailStrength * (0.2 + 0.8 * slope);
    microNormal = normalize(normal + microGrad * microAmp);

    // feature signals
    float isFlat = 1.0 - smoothstep(0.10, 0.25, slope);
    float isHigh = smoothstep(u_soilBlendHeight + 0.5, u_soilBlendHeight + 1.5, v_worldPos.y);
    float plateauFactor = isFlat * isHigh;
    float isGully = smoothstep(0.0, 0.02, -curvature) * (1.0 - smoothstep(0.25, 0.6, slope));
    float isSteep = smoothstep(0.3, 0.5, slope);
    float isRim   = smoothstep(0.0, 0.02, curvature);
    float rimFactor = isSteep * isRim;

    rockMask = clamp(rockMask + rimFactor * 0.10 - plateauFactor * 0.06 - isGully * 0.08, 0.0, 1.0);

    vec3 terrainColor = mix(soilBlend, rockColor, rockMask);

    // albedo jitter
    float jitter = (hash21(worldCoord * 0.27 + vec2(17.0, 9.0)) - 0.5) * 0.06;
    terrainColor *= (1.0 + jitter) * u_tint;

    // lighting
    vec3 L = normalize(u_lightDir);
    float ndl = max(dot(microNormal, L), 0.0);
    
    // Roughness variation based on moisture and patches (very dramatic range)
    float roughness = 0.35 + moisture * 0.30 - mudPatch * 0.35;
    float ambient = clamp(roughness, 0.15, 0.70);
    
    float fresnel = pow(1.0 - max(dot(microNormal, vec3(0.0,1.0,0.0)), 0.0), 2.0);
    float shade = ambient + ndl * 0.75 + fresnel * 0.12;

    float plateauBrightness = 1.0 + plateauFactor * 0.05;
    float gullyDarkness     = 1.0 - isGully * 0.04;
    float rimContrast       = 1.0 + rimFactor * 0.03;

    terrainColor *= plateauBrightness * gullyDarkness * rimContrast;
    vec3 litColor = terrainColor * shade * u_ambientBoost;

    FragColor = vec4(clamp(litColor, 0.0, 1.0), 1.0);
}
