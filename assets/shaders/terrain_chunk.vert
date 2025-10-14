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

// Simple hash function for noise
float hash21(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

// Value noise
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

// Low-octave fBM (2 octaves for smooth, broad displacement)
float fbm2(vec2 p) {
  float value = 0.0;
  float amplitude = 0.5;
  for (int i = 0; i < 2; ++i) {
    value += noise21(p) * amplitude;
    p = p * 2.07 + 13.17;
    amplitude *= 0.5;
  }
  return value;
}

// 2D rotation matrix
mat2 rot2(float angle) {
  float c = cos(angle);
  float s = sin(angle);
  return mat2(c, -s, s, c);
}

void main() {
  // Transform to world space
  vec3 wp = (u_model * vec4(a_position, 1.0)).xyz;
  vec3 worldNormal = normalize(mat3(u_model) * a_normal);

  // Generate stable rotation angle from noise offset
  float angle =
      fract(sin(dot(u_noiseOffset, vec2(12.9898, 78.233))) * 43758.5453) *
      6.2831853;

  // Rotate noise coordinates for variation
  vec2 uv = rot2(angle) * (wp.xz + u_noiseOffset);

  // Sample low-octave fBM for displacement (2 octaves)
  float h = fbm2(uv * u_heightNoiseFrequency) * 2.0 - 1.0;

  // Flatness factor: high on plateaus/flat areas, low on steep faces
  float flatness = clamp(worldNormal.y, 0.0, 1.0);

  // More displacement on flat areas (plateaus), less on steep slopes
  float displacementFactor = mix(0.4, 1.0, flatness);

  // Clamp height noise strength to reasonable range (0.10-0.20 world units)
  float heightAmp = clamp(u_heightNoiseStrength, 0.0, 0.20);

  // Apply displacement
  float displacement = h * heightAmp * displacementFactor;
  wp.y += displacement;

  v_worldPos = wp;
  v_normal = worldNormal;
  v_uv = a_uv;
  v_vertexDisplacement = displacement;

  gl_Position = u_mvp * vec4(wp, 1.0);
}
