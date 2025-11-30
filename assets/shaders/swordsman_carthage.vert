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
out float v_layerNoise;
out float v_plateStress;
out float v_lamellaPhase;
out float v_latitudeMix;
out float v_cuirassProfile;
out float v_chainmailMix;
out float v_frontMask;

float hash13(vec3 p) {
  return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
}

vec3 fallbackUp(vec3 n) {
  return (abs(n.y) > 0.92) ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
}

void main() {
  vec3 position = a_position;
  vec3 normal = a_normal;

  // Only annotate helmet flag; shield bending removed to keep mesh intact.
  bool is_shield = (u_materialId == 4);
  bool is_helmet = (u_materialId == 2);

  mat3 normalMatrix = mat3(transpose(inverse(u_model)));
  vec3 worldNormal = normalize(normalMatrix * normal);

  vec3 t = normalize(cross(fallbackUp(worldNormal), worldNormal));
  if (length(t) < 1e-4)
    t = vec3(1.0, 0.0, 0.0);
  t = normalize(t - worldNormal * dot(worldNormal, t));
  vec3 b = normalize(cross(worldNormal, t));

  vec4 modelPos = u_model * vec4(position, 1.0);
  vec3 worldPos = modelPos.xyz;

  float dentSeed = hash13(worldPos * 0.8 + worldNormal * 0.3);
  float torsion = 0.0; // removed bulk shear to avoid squashing

  // Minimal acne offset; no sculpting except shield bend above.
  vec3 offsetPos = worldPos + worldNormal * 0.003;

  mat4 invModel = inverse(u_model);
  vec4 localPosition = invModel * vec4(offsetPos, 1.0);
  gl_Position = u_mvp * localPosition;

  v_worldPos = offsetPos;
  v_texCoord = a_texCoord;
  v_normal = worldNormal;
  v_worldNormal = worldNormal;
  v_tangent = t;
  v_bitangent = b;

  vec3 localPos = localPosition.xyz;
  float localHeight = clamp((localPos.y + 0.30) / 1.45, 0.0, 1.0);
  float chestSplit = smoothstep(0.35, 0.78, localHeight);
  float ribWave = sin((localPos.y + 0.15) * 8.5 + localPos.x * 2.1);
  float abWave = sin((localHeight - 0.35) * 12.0 - localPos.x * 1.7);
  float sculpt = mix(ribWave, abWave, chestSplit);
  v_cuirassProfile = clamp(0.5 + 0.45 * sculpt, 0.0, 1.0);

  float mailBands = smoothstep(0.15, 0.60, 1.0 - localHeight);
  float mailPattern = 0.5 + 0.5 * sin(localPos.x * 9.0 + localPos.y * 6.2);
  v_chainmailMix = clamp(mailBands * mailPattern, 0.0, 1.0);

  v_frontMask = clamp(smoothstep(-0.18, 0.18, -localPos.z), 0.0, 1.0);

  float armorHeight = offsetPos.y;
  if (armorHeight > 1.5) {
    v_armorLayer = 0.0;
  } else if (armorHeight > 0.8) {
    v_armorLayer = 1.0;
  } else {
    v_armorLayer = 2.0;
  }

  float torsoMin = 0.58;
  float torsoMax = 1.50;
  v_bodyHeight =
      clamp((offsetPos.y - torsoMin) / (torsoMax - torsoMin), 0.0, 1.0);
  v_layerNoise = dentSeed;
  v_plateStress = torsion;
  v_lamellaPhase = fract(offsetPos.y * 4.25 + offsetPos.x * 0.12);
  v_latitudeMix = clamp((offsetPos.z + 2.5) * 0.2, 0.0, 1.0);
}
