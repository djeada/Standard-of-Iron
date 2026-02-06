#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;
layout(location = 3) in vec4 a_instanceModelCol0;
layout(location = 4) in vec4 a_instanceModelCol1;
layout(location = 5) in vec4 a_instanceModelCol2;
layout(location = 6) in vec4 a_instanceColorAlpha;

uniform mat4 u_mvp;
uniform mat4 u_model;
uniform mat4 u_viewProj;
uniform bool u_instanced;
uniform int u_materialId;

out vec3 v_normal;
out vec3 v_worldNormal;
out vec3 v_tangent;
out vec3 v_bitangent;
out vec2 v_texCoord;
out vec3 v_worldPos;
out vec3 v_instanceColor;
out float v_instanceAlpha;
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
  mat4 eff_model;
  if (u_instanced) {
    eff_model = mat4(vec4(a_instanceModelCol0.xyz, 0.0),
                     vec4(a_instanceModelCol1.xyz, 0.0),
                     vec4(a_instanceModelCol2.xyz, 0.0),
                     vec4(a_instanceModelCol0.w, a_instanceModelCol1.w,
                          a_instanceModelCol2.w, 1.0));
    v_instanceColor = a_instanceColorAlpha.rgb;
    v_instanceAlpha = a_instanceColorAlpha.a;
  } else {
    eff_model = u_model;
    v_instanceColor = vec3(0.0);
    v_instanceAlpha = 1.0;
  }
  vec3 position = a_position;
  vec3 normal = a_normal;

  bool is_shield = (u_materialId == 4);
  bool is_helmet = (u_materialId == 2);

  mat3 normalMatrix = mat3(eff_model);
  vec3 worldNormal = normalize(normalMatrix * normal);

  vec3 t = normalize(cross(fallbackUp(worldNormal), worldNormal));
  if (length(t) < 1e-4)
    t = vec3(1.0, 0.0, 0.0);
  t = normalize(t - worldNormal * dot(worldNormal, t));
  vec3 b = normalize(cross(worldNormal, t));

  vec4 modelPos = eff_model * vec4(position, 1.0);
  vec3 worldPos = modelPos.xyz;

  vec3 axisY = vec3(eff_model[1].xyz);
  float axisLen = max(length(axisY), 1e-4);
  vec3 axisDir = axisY / axisLen;
  vec3 modelOrigin = vec3(eff_model[3].xyz);
  float height01 =
      clamp(dot(worldPos - modelOrigin, axisDir) / axisLen + 0.5, 0.0, 1.0);

  float dentSeed = hash13(worldPos * 0.8 + worldNormal * 0.3);
  float torsion = 0.0;

  vec3 offsetPos = worldPos + worldNormal * 0.003;

  gl_Position = u_viewProj * vec4(offsetPos, 1.0);

  v_worldPos = offsetPos;
  v_texCoord = a_texCoord;
  v_normal = worldNormal;
  v_worldNormal = worldNormal;
  v_tangent = t;
  v_bitangent = b;

  vec3 localPos = a_position;
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

  v_armorLayer = (u_materialId == 1) ? 1.0 : 0.0;

  v_bodyHeight = clamp((height01 - 0.05) / 0.90, 0.0, 1.0);
  v_layerNoise = dentSeed;
  v_plateStress = torsion;
  v_lamellaPhase = fract(offsetPos.y * 4.25 + offsetPos.x * 0.12);
  v_latitudeMix = clamp((offsetPos.z + 2.5) * 0.2, 0.0, 1.0);
}
