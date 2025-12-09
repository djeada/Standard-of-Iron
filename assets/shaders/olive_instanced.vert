#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec4 aPosScale;
layout(location = 4) in vec4 aColorSway;
layout(location = 5) in vec4 aRotation;

uniform mat4 uViewProj;
uniform float uTime;
uniform float uWindStrength;
uniform float uWindSpeed;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;
out vec2 vTexCoord;
out float vFoliageMask;
out float vLeafSeed;
out float vBarkSeed;
out float vBranchId;
out vec2 vLocalPosXZ;

void main() {
  const float TWO_PI = 6.2831853;

  float scale = aPosScale.w;
  vec3 worldPos = aPosScale.xyz;
  float swayPhase = aColorSway.a;
  float rotation = aRotation.x;
  float silhouetteSeed = aRotation.y;
  float leafSeed = aRotation.z;
  float barkSeed = aRotation.w;

  vec3 modelPos = aPos;

  float trunkMask = 1.0 - smoothstep(0.12, 0.20, aTexCoord.y);
  float foliageMask = smoothstep(0.45, 0.55, aTexCoord.y);

  float angle = aTexCoord.x * TWO_PI;
  float branchId = floor(angle / TWO_PI * 4.0 + silhouetteSeed * 4.0);

  if (trunkMask > 0.0) {
    float twist = sin(aTexCoord.y * 20.0 + barkSeed * TWO_PI) * 0.02;
    modelPos.x += twist * trunkMask;
    modelPos.z += twist * 0.7 * trunkMask;
  }

  vec3 localPos = modelPos * scale;

  float heightFactor = clamp(aPos.y * 2.0, 0.0, 1.0);
  float windTime = uTime * uWindSpeed * 0.4;
  float sway = sin(windTime + swayPhase) * uWindStrength * 0.3;
  float sway2 = sin(windTime * 1.7 + swayPhase * 2.3) * uWindStrength * 0.15;

  float swayAmount = mix(0.02, 0.12, foliageMask) * heightFactor;
  localPos.x += sway * swayAmount;
  localPos.z += sway2 * swayAmount * 0.6;

  float cosR = cos(rotation);
  float sinR = sin(rotation);
  mat2 rot = mat2(cosR, -sinR, sinR, cosR);

  vec2 rotatedXZ = rot * localPos.xz;
  localPos = vec3(rotatedXZ.x, localPos.y, rotatedXZ.y);

  vec2 rotatedNormalXZ = rot * aNormal.xz;
  vec3 finalNormal =
      normalize(vec3(rotatedNormalXZ.x, aNormal.y, rotatedNormalXZ.y));

  vWorldPos = localPos + worldPos;
  vNormal = finalNormal;
  vColor = aColorSway.rgb;
  vTexCoord = aTexCoord;
  vFoliageMask = foliageMask;
  vLeafSeed = leafSeed;
  vBarkSeed = barkSeed;
  vBranchId = branchId;
  vLocalPosXZ = modelPos.xz;

  gl_Position = uViewProj * vec4(vWorldPos, 1.0);
}
