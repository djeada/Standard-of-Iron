#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

uniform mat4 u_mvp;
uniform mat4 u_model;
uniform float u_time;

out vec3 v_normal;
out vec3 v_worldNormal;
out vec3 v_tangent;
out vec3 v_bitangent;
out vec2 v_texCoord;
out vec3 v_worldPos;
out float v_armorLayer;
out float v_leatherTension;

float hash13(vec3 p) {
  return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
}

vec3 hash33(vec3 p) {
  p = vec3(dot(p, vec3(127.1, 311.7, 74.7)),
           dot(p, vec3(269.5, 183.3, 246.1)),
           dot(p, vec3(113.5, 271.9, 124.6)));
  return fract(sin(p) * 43758.5453123);
}

float noise3d(vec3 p) {
  vec3 i = floor(p);
  vec3 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  
  float n000 = hash13(i + vec3(0.0, 0.0, 0.0));
  float n100 = hash13(i + vec3(1.0, 0.0, 0.0));
  float n010 = hash13(i + vec3(0.0, 1.0, 0.0));
  float n110 = hash13(i + vec3(1.0, 1.0, 0.0));
  float n001 = hash13(i + vec3(0.0, 0.0, 1.0));
  float n101 = hash13(i + vec3(1.0, 0.0, 1.0));
  float n011 = hash13(i + vec3(0.0, 1.0, 1.0));
  float n111 = hash13(i + vec3(1.0, 1.0, 1.0));
  
  float x00 = mix(n000, n100, f.x);
  float x10 = mix(n010, n110, f.x);
  float x01 = mix(n001, n101, f.x);
  float x11 = mix(n011, n111, f.x);
  
  float y0 = mix(x00, x10, f.y);
  float y1 = mix(x01, x11, f.y);
  
  return mix(y0, y1, f.z);
}

vec3 fallbackUp(vec3 normal) {
  return (abs(normal.y) > 0.92) ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
}

void main() {
  mat3 normalMatrix = mat3(transpose(inverse(u_model)));
  vec3 worldNormal = normalize(normalMatrix * a_normal);

  vec3 tangentAxis = normalize(cross(fallbackUp(worldNormal), worldNormal));
  if (length(tangentAxis) < 1e-4) {
    tangentAxis = vec3(1.0, 0.0, 0.0);
  }
  vec3 bitangentAxis = normalize(cross(worldNormal, tangentAxis));

  vec4 modelPos = u_model * vec4(a_position, 1.0);
  vec3 worldPos = modelPos.xyz;
  
  float noiseVal = noise3d(worldPos * 8.0 + vec3(u_time * 0.1));
  vec3 perturbation = worldNormal * noiseVal * 0.002;
  
  vec3 offsetPos = worldPos + worldNormal * 0.008 + perturbation;

  gl_Position = u_mvp * vec4(offsetPos, 1.0);

  v_worldPos = offsetPos;
  v_texCoord = a_texCoord;
  v_normal = worldNormal;
  v_worldNormal = worldNormal;
  v_tangent = tangentAxis;
  v_bitangent = bitangentAxis;

  float height = offsetPos.y;
  float layer = 2.0;
  if (height > 1.28) {
    layer = 0.0;
  } else if (height > 0.86) {
    layer = 1.0;
  }
  v_armorLayer = layer;

  float tensionSeed = hash13(offsetPos * 0.35 + worldNormal);
  v_leatherTension = mix(tensionSeed, 1.0 - tensionSeed, layer * 0.33);
}
