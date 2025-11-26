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
out float v_bodyHeight;
out float v_layerNoise;
out float v_bendAmount;

float hash13(vec3 p) {
  return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
}

vec3 fallbackUp(vec3 normal) {
  return (abs(normal.y) > 0.92) ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
}

void main() {
  mat3 normalMatrix = mat3(transpose(inverse(u_model)));
  vec3 worldNormal = normalize(normalMatrix * a_normal);

  vec3 t = normalize(cross(fallbackUp(worldNormal), worldNormal));
  if (length(t) < 1e-4)
    t = vec3(1.0, 0.0, 0.0);
  t = normalize(t - worldNormal * dot(worldNormal, t));
  vec3 b = normalize(cross(worldNormal, t));

  vec4 modelPos = u_model * vec4(a_position, 1.0);
  vec3 worldPos = modelPos.xyz;

  float dentNoise = hash13(worldPos * 0.85 + worldNormal * 0.25);
  float torsion = sin(worldPos.y * 11.5 + dentNoise * 6.28318);
  vec3 dentOffset = worldNormal * ((dentNoise - 0.5) * 0.012);
  vec3 shearAxis = normalize(vec3(worldNormal.z, 0.15, -worldNormal.x));
  vec3 shearOffset = shearAxis * torsion * 0.004;
  vec3 batteredPos = worldPos + dentOffset + shearOffset;

  // Extra shaping for helmet region (top of character).
  float height = batteredPos.y;
  float helmetMask = smoothstep(0.60, 0.95, height);
  float rimMask =
      smoothstep(0.68, 0.92, height) * (1.0 - smoothstep(0.96, 1.12, height));
  float tipMask = smoothstep(1.08, 1.28, height);
  vec3 radial =
      normalize(vec3(batteredPos.x, 0.0, batteredPos.z) + vec3(0.0001));
  vec3 rimFlare = radial * (0.08 * rimMask * helmetMask);
  vec3 tipTaper = radial * (-0.07 * tipMask * helmetMask);

  vec3 offsetPos = batteredPos + worldNormal * 0.006 + rimFlare + tipTaper;
  mat4 invModel = inverse(u_model);
  vec4 localOffset = invModel * vec4(offsetPos, 1.0);
  gl_Position = u_mvp * localOffset;

  v_worldPos = offsetPos;
  v_texCoord = a_texCoord;
  v_normal = worldNormal;
  v_worldNormal = worldNormal;
  v_tangent = t;
  v_bitangent = b;

  height = offsetPos.y;
  float layer = 2.0;
  if (height > 1.28)
    layer = 0.0;
  else if (height > 0.86)
    layer = 1.0;
  v_armorLayer = layer;

  float tensionSeed = hash13(offsetPos * 0.35 + worldNormal * 1.7);
  float heightFactor = smoothstep(0.5, 1.5, height);
  float curvatureFactor = length(vec2(worldNormal.x, worldNormal.z));
  v_leatherTension = mix(tensionSeed, 1.0 - tensionSeed, layer * 0.42) *
                     (0.65 + curvatureFactor * 0.35) *
                     (0.78 + heightFactor * 0.22);

  float torsoMin = 0.58;
  float torsoMax = 1.36;
  v_bodyHeight =
      clamp((offsetPos.y - torsoMin) / (torsoMax - torsoMin), 0.0, 1.0);
  v_layerNoise = dentNoise;
  v_bendAmount = torsion;
}
