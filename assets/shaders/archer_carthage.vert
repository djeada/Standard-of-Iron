#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

uniform mat4 u_mvp;
uniform mat4 u_model;

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
  vec3 offsetPos = worldPos + worldNormal * 0.008;

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
