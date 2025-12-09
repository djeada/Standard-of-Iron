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
out float v_helmetDetail;
out float v_platePhase;
out float v_segmentStress;
out float v_rivetPattern;
out float v_leatherWear;
out float v_polishLevel;

float hash13(vec3 p) {
  return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
}

vec3 fallbackUp(vec3 n) {
  return (abs(n.y) > 0.92) ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
}

void main() {
  vec3 position = a_position;
  vec3 normal = a_normal;

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

  vec3 t = normalize(cross(fallbackUp(worldNormal), worldNormal));
  if (length(t) < 1e-4)
    t = vec3(1.0, 0.0, 0.0);
  t = normalize(t - worldNormal * dot(worldNormal, t));
  vec3 b = normalize(cross(worldNormal, t));

  vec4 modelPos = u_model * vec4(position, 1.0);
  vec3 worldPos = modelPos.xyz;

  bool deformArmor =
      (u_materialId == 1 || u_materialId == 2 || u_materialId == 4 ||
       u_materialId == 3 || u_materialId == 5);

  float dentSeed = 0.0;
  float combatStress = 0.0;
  vec3 batteredPos = worldPos;
  vec3 offsetPos = worldPos;

  if (deformArmor) {

    dentSeed = hash13(worldPos * 0.88 + worldNormal * 0.32);
    combatStress = sin(worldPos.y * 18.2 + dentSeed * 22.61);
    vec3 dentOffset = worldNormal * ((dentSeed - 0.5) * 0.0105);
    vec3 shearAxis = normalize(vec3(worldNormal.z, 0.25, -worldNormal.x));
    vec3 shearOffset = shearAxis * combatStress * 0.0038;

    batteredPos = worldPos + dentOffset + shearOffset;
    offsetPos = batteredPos + worldNormal * 0.0062;
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
  v_armorLayer = (u_materialId == 1) ? 1.0 : 0.0;

  float torsoMin = 0.55;
  float torsoMax = 1.70;
  v_bodyHeight =
      clamp((offsetPos.y - torsoMin) / (torsoMax - torsoMin), 0.0, 1.0);

  float reinforcementBands = fract(height * 14.0);
  float browBandRegion =
      smoothstep(1.48, 1.52, height) * smoothstep(1.56, 1.52, height);
  float cheekGuardArea =
      smoothstep(1.42, 1.52, height) * smoothstep(1.68, 1.58, height);
  v_helmetDetail =
      reinforcementBands * 0.35 + browBandRegion * 0.45 + cheekGuardArea * 0.2;

  v_platePhase = fract(height * 6.5);

  float localX = (invModel * vec4(offsetPos, 1.0)).x;
  v_segmentStress = combatStress * (0.6 + 0.4 * sin(localX * 12.0));

  v_rivetPattern = step(0.96, fract(offsetPos.x * 18.0)) *
                   step(0.94, fract(v_platePhase * 1.0));

  v_leatherWear = hash13(offsetPos * 0.48 + worldNormal * 2.1) *
                  (0.55 + v_bodyHeight * 0.45);

  v_polishLevel = 1.0 - dentSeed * 0.4 - v_bodyHeight * 0.2;
}
