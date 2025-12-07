#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

uniform mat4 u_mvp;
uniform mat4 u_model;
uniform int u_materialId;
uniform float u_time;

out vec3 v_normal;
out vec3 v_worldNormal;
out vec3 v_tangent;
out vec3 v_bitangent;
out vec2 v_texCoord;
out vec3 v_worldPos;
out float v_armorLayer;
out float v_bodyHeight;
out float v_helmetDetail;
out float v_chainmailPhase;
out float v_leatherWear;
out float v_curvature;

float hash13(vec3 p) {
  return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
}

vec3 fallbackUp(vec3 n) {
  return (abs(n.y) > 0.92) ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
}

void main() {
  vec3 position = a_position;
  vec3 normal = a_normal;

  // Cloak back drape (Material ID 5)
  if (u_materialId == 5) {
    float v = 1.0 - a_texCoord.y; // 1 = top, 0 = bottom
    float u = a_texCoord.x;
    float x_norm = (u - 0.5) * 2.0;

    float pin = smoothstep(0.85, 1.0, v);
    float move = 1.0 - pin;

    float top_blend = smoothstep(0.90, 1.0, v);
    float edge_emphasis = abs(x_norm);
    float shoulder_wrap = top_blend * (0.42 + edge_emphasis * 0.55);
    position.y -= shoulder_wrap * 0.08;
    position.z += shoulder_wrap * 0.08;

    float bottom_blend = smoothstep(0.0, 0.30, 1.0 - v);
    position.y += bottom_blend * 0.08;
    position.x += sign(position.x) * bottom_blend * 0.10;

    float wave = sin(position.z * 5.0 + x_norm * 0.5 + u_time * 1.6) * 0.02;
    position.y += wave * move;
  }

  // Cloak shoulder cape (Material ID 6)
  if (u_materialId == 6) {
    float u = a_texCoord.x;
    float v = a_texCoord.y;
    float x_norm = (u - 0.5) * 2.0;
    float x_abs = abs(x_norm);
    float z_norm = (v - 0.5) * 2.0;

    float shoulder_droop = x_abs * x_abs * 0.10;
    position.y -= shoulder_droop;

    float back_droop = max(0.0, z_norm) * max(0.0, z_norm) * 0.06;
    position.y -= back_droop;

    float front_droop = max(0.0, -z_norm) * max(0.0, -z_norm) * 0.03;
    position.y -= front_droop;

    float flutter = sin(u_time * 2.0 + x_norm * 3.0 + z_norm * 2.0) * 0.005;
    position.y += flutter;
  }

  // Shield curving: bend flat rectangle into scutum curve (materialId=4)
  if (u_materialId == 4) {
    float curveRadius = 0.55;
    float curveAmount = 0.45;
    float angle = position.x * curveAmount;

    float curved_x = sin(angle) * curveRadius;
    float curved_z = position.z + (1.0 - cos(angle)) * curveRadius;
    position = vec3(curved_x, position.y, curved_z);

    normal = vec3(sin(angle) * normal.z + cos(angle) * normal.x, normal.y,
                  cos(angle) * normal.z - sin(angle) * normal.x);
  }

  mat3 normalMatrix = mat3(transpose(inverse(u_model)));
  vec3 worldNormal = normalize(normalMatrix * normal);

  // Build tangent space
  vec3 t = normalize(cross(fallbackUp(worldNormal), worldNormal));
  if (length(t) < 1e-4)
    t = vec3(1.0, 0.0, 0.0);
  t = normalize(t - worldNormal * dot(worldNormal, t));
  vec3 b = normalize(cross(worldNormal, t));

  vec4 modelPos = u_model * vec4(position, 1.0);
  vec3 worldPos = modelPos.xyz;

  // Only add battle-wear deformation to armored pieces (not skin or cloth)
  bool deformArmor = (u_materialId == 1 || u_materialId == 2 ||
                      u_materialId == 4 || u_materialId == 3);

  vec3 batteredPos = worldPos;
  vec3 offsetPos = worldPos;

  if (deformArmor) {
    // Procedural denting and battle damage for armor/helmet/shield/weapons
    float dentSeed = hash13(worldPos * 0.75 + worldNormal * 0.22);
    float hammerNoise = sin(worldPos.y * 14.3 + dentSeed * 12.56);
    vec3 dentOffset = worldNormal * ((dentSeed - 0.5) * 0.0095);
    vec3 shearAxis = normalize(vec3(worldNormal.z, 0.18, -worldNormal.x));
    vec3 shearOffset = shearAxis * hammerNoise * 0.0032;

    batteredPos = worldPos + dentOffset + shearOffset;
    offsetPos = batteredPos + worldNormal * 0.0055;
  }

  mat4 invModel = inverse(u_model);
  vec4 localBattered = invModel * vec4(batteredPos, 1.0);
  gl_Position = u_mvp * localBattered;

  v_worldPos = offsetPos;
  v_texCoord = a_texCoord;
  v_normal = worldNormal;
  v_worldNormal = worldNormal;
  v_tangent = t;
  v_bitangent = b;

  float height = offsetPos.y;
  // Keep armor/material selection stable: 1.0 only for armor material.
  v_armorLayer = (u_materialId == 1) ? 1.0 : 0.0;

  // Body height normalization
  float torsoMin = 0.55;
  float torsoMax = 1.65;
  v_bodyHeight =
      clamp((offsetPos.y - torsoMin) / (torsoMax - torsoMin), 0.0, 1.0);

  // Light helmet detail attributes
  float conicalHeight = smoothstep(1.55, 1.75, height);
  float browBandRegion =
      smoothstep(1.48, 1.52, height) * smoothstep(1.56, 1.52, height);
  v_helmetDetail = conicalHeight * 0.6 + browBandRegion * 0.4;

  // Chainmail ring phase
  v_chainmailPhase = fract(offsetPos.x * 32.0 + offsetPos.z * 32.0);

  // Leather wear and tension
  float tensionSeed = hash13(offsetPos * 0.42 + worldNormal * 1.5);
  v_leatherWear = tensionSeed * (0.7 + v_bodyHeight * 0.3);

  // Surface curvature indicator
  v_curvature = length(vec2(worldNormal.x, worldNormal.z));
}
