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
out float v_steelWear;
out float v_chainmailPhase;
out float v_rivetPattern;
out float v_leatherWear;

float hash13(vec3 p) {
  return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
}

vec3 fallbackUp(vec3 n) {
  return (abs(n.y) > 0.92) ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
}

void main() {
  vec3 position = a_position;
  vec3 normal = a_normal;

  // Slight curvature for large shields (materialId = 4)
  if (u_materialId == 4) {
    float curveRadius = 0.52;
    float curveAmount = 0.46;
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
  vec3 axisY = vec3(u_model[1].xyz);
  float axisLen = max(length(axisY), 1e-4);
  vec3 axisDir = axisY / axisLen;
  vec3 modelOrigin = vec3(u_model[3].xyz);
  float height01 =
      clamp(dot(worldPos - modelOrigin, axisDir) / axisLen + 0.5, 0.0, 1.0);

  // Only deform armored pieces (helmet/armor/shield/weapons) and only within
  // their height bands to avoid touching skin/face
  bool torsoZone = (height01 > 0.18 && height01 <= 0.98);
  bool helmetZone = (height01 > 0.70); // allow small overlap for helmet rim
  bool deformArmor =
      (u_materialId == 4 || u_materialId == 3 || // shields, weapons
       (u_materialId == 1 && torsoZone) ||       // chainmail
       (u_materialId == 2 && helmetZone));       // helmet

  float dentSeed = 0.0;
  float hammerImpact = 0.0;
  vec3 batteredPos = worldPos;
  vec3 offsetPos = worldPos;

  if (deformArmor) {
    // Subtle battered offset to avoid self-shadowing
    dentSeed = hash13(worldPos * 0.82 + worldNormal * 0.28);
    hammerImpact = sin(worldPos.y * 15.0 + dentSeed * 18.84);
    vec3 dentOffset = worldNormal * ((dentSeed - 0.5) * 0.0095);
    vec3 shearAxis = normalize(vec3(worldNormal.z, 0.22, -worldNormal.x));
    vec3 shearOffset = shearAxis * hammerImpact * 0.0038;

    batteredPos = worldPos + dentOffset + shearOffset;
    offsetPos = batteredPos + worldNormal * 0.006;
  }

  mat4 invModel = inverse(u_model);
  vec4 localOffset = invModel * vec4(offsetPos, 1.0);
  gl_Position = u_mvp * localOffset;

  v_worldPos = offsetPos;
  v_texCoord = a_texCoord;
  v_normal = worldNormal;
  v_worldNormal = worldNormal;
  v_tangent = t;
  v_bitangent = b;

  float height = height01;

  // Keep armor material active across the whole piece; avoid partial bands.
  v_armorLayer = (u_materialId == 1) ? 1.0 : 0.0;

  v_bodyHeight = clamp((height - 0.05) / 0.90, 0.0, 1.0);

  // Helmet detail bands and rivets
  float reinforcementBands = fract(height * 12.0);
  float browBandRegion =
      smoothstep(0.78, 0.82, height) * smoothstep(0.90, 0.86, height);
  float cheekGuardArea =
      smoothstep(0.70, 0.82, height) * smoothstep(0.90, 0.82, height);
  v_helmetDetail =
      reinforcementBands * 0.4 + browBandRegion * 0.4 + cheekGuardArea * 0.2;

  // Wear masks
  v_steelWear = dentSeed * (1.0 - v_bodyHeight * 0.3); // More wear lower down
  v_chainmailPhase =
      fract(offsetPos.x * 28.0 + offsetPos.z * 28.0 + offsetPos.y * 0.45);
  v_rivetPattern = step(0.96, fract(offsetPos.x * 22.0)) *
                   step(0.94, fract(offsetPos.z * 18.0));
  v_leatherWear =
      hash13(offsetPos * 0.45 + worldNormal * 1.6) * (0.6 + v_bodyHeight * 0.4);
}
