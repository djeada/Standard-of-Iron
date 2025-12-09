#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aPosScale;
layout(location = 3) in vec4 aColorRot;

uniform mat4 uViewProj;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;

void main() {
  float scale = aPosScale.w;
  vec3 worldPos = aPosScale.xyz;
  float rotation = aColorRot.a;

  float cosR = cos(rotation);
  float sinR = sin(rotation);
  mat2 rot = mat2(cosR, -sinR, sinR, cosR);

  vec3 localPos = aPos * scale;
  vec2 rotatedXZ = rot * localPos.xz;
  localPos = vec3(rotatedXZ.x, localPos.y, rotatedXZ.y);

  vWorldPos = localPos + worldPos;

  vec2 rotatedNormalXZ = rot * aNormal.xz;
  vNormal = normalize(vec3(rotatedNormalXZ.x, aNormal.y, rotatedNormalXZ.y));

  vColor = aColorRot.rgb;

  gl_Position = uViewProj * vec4(vWorldPos, 1.0);
}
