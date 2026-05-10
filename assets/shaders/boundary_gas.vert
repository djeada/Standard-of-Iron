#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

uniform mat4 u_model;
layout(std140) uniform FrameData { mat4 u_viewProj; };
uniform float u_time;

out vec2 v_texCoord;
out vec3 v_worldPos;
out vec3 v_worldNormal;
out float v_sheetDrift;

float hash(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float a = hash(i);
  float b = hash(i + vec2(1.0, 0.0));
  float c = hash(i + vec2(0.0, 1.0));
  float d = hash(i + vec2(1.0, 1.0));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main() {
  vec3 worldNormal = normalize(mat3(u_model) * a_normal);
  vec3 worldPos = (u_model * vec4(a_position, 1.0)).xyz;

  float baseNoise = noise(worldPos.xz * 0.06 + vec2(a_texCoord.y * 1.6, 0.0));
  float upperNoise = noise(worldPos.xz * 0.11 + vec2(0.0, a_texCoord.y * 2.3) -
                           u_time * 0.015);
  float driftPhase = u_time * 0.55 + worldPos.y * 0.22 + baseNoise * 4.0;

  float lateralDrift =
      sin(driftPhase) * (0.18 + 0.22 * upperNoise) * a_texCoord.y;
  float breathing = sin(u_time * 0.32 + worldPos.x * 0.04 + worldPos.z * 0.05) *
                    0.12 * (0.35 + 0.65 * a_texCoord.y);
  float verticalCurl =
      sin(u_time * 0.40 + worldPos.x * 0.05 + baseNoise * 6.0) * 0.18;

  worldPos += worldNormal * (lateralDrift + breathing);
  worldPos.y += verticalCurl * (0.18 + 0.82 * a_texCoord.y);

  v_texCoord = a_texCoord;
  v_worldPos = worldPos;
  v_worldNormal = worldNormal;
  v_sheetDrift = lateralDrift + breathing;

  gl_Position = u_viewProj * vec4(worldPos, 1.0);
}
