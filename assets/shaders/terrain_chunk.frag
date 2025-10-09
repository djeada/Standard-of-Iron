#version 330 core

in vec3 v_worldPos;
in vec3 v_normal;
in vec2 v_uv;

layout (location = 0) out vec4 FragColor;

uniform vec3 u_grassPrimary;
uniform vec3 u_grassSecondary;
uniform vec3 u_grassDry;
uniform vec3 u_soilColor;
uniform vec3 u_rockLow;
uniform vec3 u_rockHigh;
uniform vec3 u_tint;
uniform vec2 u_noiseOffset;
uniform float u_tileSize;
uniform float u_macroNoiseScale;
uniform float u_detailNoiseScale;
uniform float u_slopeRockThreshold;
uniform float u_slopeRockSharpness;
uniform float u_soilBlendHeight;
uniform float u_soilBlendSharpness;
uniform float u_heightNoiseStrength;
uniform float u_heightNoiseFrequency;
uniform float u_ambientBoost;
uniform float u_rockDetailStrength;
uniform vec3 u_lightDir;

float hash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise21(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);

    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(vec2 p) {
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 4; ++i) {
        value += noise21(p) * amplitude;
        p = p * 2.07 + 13.17;
        amplitude *= 0.5;
    }
    return value;
}

void main() {
    vec3 normal = normalize(v_normal);
    vec3 upDir = vec3(0.0, 1.0, 0.0);
    float slope = clamp(1.0 - max(dot(normal, upDir), 0.0), 0.0, 1.0);

    float tileScale = max(u_tileSize, 0.0001);
    vec2 worldCoord = (v_worldPos.xz / tileScale) + u_noiseOffset;

    float macroNoise = fbm(worldCoord * u_macroNoiseScale);
    float detailNoise = noise21(worldCoord * (u_detailNoiseScale * 2.5));
    float erosionNoise = noise21(worldCoord * (u_detailNoiseScale * 4.0) + 17.0);

    float lushFactor = smoothstep(0.2, 0.8, macroNoise);
    vec3 lushGrass = mix(u_grassPrimary, u_grassSecondary, lushFactor);

    float dryness = clamp(0.55 * slope + 0.45 * detailNoise, 0.0, 1.0);
    vec3 grassColor = mix(lushGrass, u_grassDry, dryness);

    float soilNoise = (noise21(worldCoord * (u_heightNoiseFrequency * 6.0) + 9.7) - 0.5) * u_heightNoiseStrength;
    float soilWidth = max(0.01, 1.0 / max(u_soilBlendSharpness, 0.001));
    float soilEdgeA = u_soilBlendHeight - soilNoise;
    float soilEdgeB = u_soilBlendHeight + soilWidth;
    float soilEdgeMin = min(soilEdgeA, soilEdgeB);
    float soilEdgeMax = max(soilEdgeA, soilEdgeB);
    float soilMix = 1.0 - smoothstep(soilEdgeMin, soilEdgeMax, v_worldPos.y);
    soilMix = clamp(soilMix, 0.0, 1.0);
    vec3 soilBlend = mix(grassColor, u_soilColor, soilMix);

    float slopeCut = smoothstep(u_slopeRockThreshold,
                                u_slopeRockThreshold + max(0.02, 1.0 / max(u_slopeRockSharpness, 0.1)),
                                slope);
    float rockMask = clamp(pow(slopeCut, u_slopeRockSharpness) +
                           (erosionNoise - 0.5) * u_rockDetailStrength,
                           0.0, 1.0);

    float rockLerp = clamp(0.35 + detailNoise * 0.65, 0.0, 1.0);
    vec3 rockColor = mix(u_rockLow, u_rockHigh, rockLerp);
    rockColor = mix(rockColor, rockColor * 1.15, clamp(u_rockDetailStrength * 1.4, 0.0, 1.0));

    vec3 terrainColor = mix(soilBlend, rockColor, rockMask);
    terrainColor *= u_tint;

    vec3 lightDir = normalize(u_lightDir);
    float ndl = max(dot(normal, lightDir), 0.0);
    float ambient = 0.35;
    float fresnel = pow(1.0 - max(dot(normal, normalize(vec3(0.0, 1.0, 0.0))), 0.0), 2.0);

    float shade = ambient + ndl * 0.75 + fresnel * 0.12;
    vec3 litColor = terrainColor * shade * u_ambientBoost;

    FragColor = vec4(clamp(litColor, 0.0, 1.0), 1.0);
}
