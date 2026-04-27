#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;

uniform mat4 u_mvp;
uniform mat4 u_model;
uniform vec2 u_noiseOffset;
uniform float u_heightNoiseStrength;
uniform float u_heightNoiseFrequency;

out vec3 v_worldPos;
out vec3 v_normal;
out vec2 v_uv;
out float v_vertexDisplacement;
out float v_entryMask;

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

float fbm3(vec2 p) {
  float value = 0.0;
  float amplitude = 0.5;
  for (int i = 0; i < 3; ++i) {
    value += noise21(p) * amplitude;
    p = p * 2.13 + 11.47;
    amplitude *= 0.48;
  }
  return value;
}

mat2 rot2(float angle) {
  float c = cos(angle);
  float s = sin(angle);
  return mat2(c, -s, s, c);
}

float sampleTerrainDisplacement(vec3 wp, vec3 worldNormal, vec2 noiseOffset,
                                float heightNoiseStrength,
                                float heightNoiseFrequency, float entryMask) {
  float frequency = max(heightNoiseFrequency, 0.0001);
  float angle =
      fract(sin(dot(noiseOffset, vec2(12.9898, 78.233))) * 43758.5453) *
      6.2831853;
  vec2 uv = rot2(angle) * (wp.xz + noiseOffset);

  float h0 = fbm3(uv * frequency * 0.55 + vec2(19.1, -7.3)) * 2.0 - 1.0;
  float h1 = fbm3(uv * frequency) * 2.0 - 1.0;
  float h2 = fbm3(uv * frequency * 2.7) * 2.0 - 1.0;
  float h = h0 * 0.45 + h1 * 0.35 + h2 * 0.20;

  float flatness = clamp(worldNormal.y, 0.0, 1.0);
  float slope = 1.0 - flatness;
  float shoulderMask = smoothstep(0.04, 0.32, slope);
  shoulderMask = mix(shoulderMask, 1.0, smoothstep(0.55, 0.85, slope) * 0.35);
  shoulderMask *= 1.0 - 0.60 * entryMask;

  float heightAmp = clamp(heightNoiseStrength * 1.35, 0.0, 0.22);
  return h * heightAmp * shoulderMask;
}

void main() {
  vec3 baseWp = (u_model * vec4(a_position, 1.0)).xyz;
  vec3 worldNormal = normalize(mat3(u_model) * a_normal);
  float entryMask = clamp(a_uv.y, 0.0, 1.0);

  float displacement = sampleTerrainDisplacement(
      baseWp, worldNormal, u_noiseOffset, u_heightNoiseStrength,
      u_heightNoiseFrequency, entryMask);
  vec3 wp = baseWp;
  wp.y += displacement;

  float sampleStep = 0.35;
  vec3 dx = vec3(sampleStep, 0.0, 0.0);
  vec3 dz = vec3(0.0, 0.0, sampleStep);
  vec3 px = baseWp + dx;
  vec3 pz = baseWp + dz;
  px.y += sampleTerrainDisplacement(px, worldNormal, u_noiseOffset,
                                    u_heightNoiseStrength,
                                    u_heightNoiseFrequency, entryMask);
  pz.y += sampleTerrainDisplacement(pz, worldNormal, u_noiseOffset,
                                    u_heightNoiseStrength,
                                    u_heightNoiseFrequency, entryMask);
  vec3 displacedNormal = normalize(cross(pz - wp, px - wp));
  if (dot(displacedNormal, worldNormal) < 0.0) {
    displacedNormal = -displacedNormal;
  }

  v_worldPos = wp;
  v_normal = normalize(mix(worldNormal, displacedNormal, 0.75));
  v_uv = a_uv;
  v_vertexDisplacement = displacement;
  v_entryMask = entryMask;

  gl_Position = u_mvp * vec4(wp, 1.0);
}
