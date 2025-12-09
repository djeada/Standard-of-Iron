#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

uniform mat4 u_mvp;
uniform mat4 u_model;
uniform int u_materialId;

out vec3 v_normal;
out vec3 v_worldNormal;
out vec3 v_tangent;
out vec3 v_bitangent;
out vec2 v_texCoord;
out vec3 v_worldPos;
out float v_armorLayer;
out float v_bodyHeight;
out float v_armorSheen;
out float v_leatherWear;
out float v_horseMusculature;
out float v_hairFlow;
out float v_hoofWear;

float hash13(vec3 p) {
  return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
}

vec3 fallbackUp(vec3 n) {
  return (abs(n.y) > 0.92) ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
}

void main() {
  vec3 position = a_position;
  vec3 normal = a_normal;

  mat3 normalMatrix = mat3(transpose(inverse(u_model)));
  vec3 worldNormal = normalize(normalMatrix * normal);

  vec3 t = normalize(cross(fallbackUp(worldNormal), worldNormal));
  if (length(t) < 1e-4)
    t = vec3(1.0, 0.0, 0.0);
  t = normalize(t - worldNormal * dot(worldNormal, t));
  vec3 b = normalize(cross(worldNormal, t));

  v_normal = normal;
  v_worldNormal = worldNormal;
  v_tangent = t;
  v_bitangent = b;
  v_texCoord = a_texCoord;
  v_worldPos = vec3(u_model * vec4(position, 1.0));

  v_bodyHeight = clamp((v_worldPos.y + 0.2) / 2.0, 0.0, 1.0);

  float hashVal = hash13(v_worldPos * 0.5);

  v_armorSheen = 0.8 + hashVal * 0.2;
  v_leatherWear = hashVal * 0.3 + 0.1;

  v_horseMusculature =
      smoothstep(0.3, 0.6, v_bodyHeight) * smoothstep(1.0, 0.7, v_bodyHeight);
  v_hairFlow = hashVal * 0.5 + 0.5;
  v_hoofWear = hashVal * 0.3;

  v_armorLayer = (u_materialId == 1) ? 1.0 : 0.0;

  gl_Position = u_mvp * vec4(position, 1.0);
}
