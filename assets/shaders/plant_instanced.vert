

#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec4 aPosScale;
layout(location = 4) in vec4 aColorSway;
layout(location = 5) in vec4 aTypeParams;

uniform mat4 uViewProj;
uniform float uTime;
uniform float uWindStrength;
uniform float uWindSpeed;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;
out vec2 vTexCoord;
out float vAlpha;
out float vHeight;
out float vSeed;
out float vType;
out vec3 vTangent;
out vec3 vBitangent;

float h11(float n) { return fract(sin(n) * 43758.5453123); }

float h31(vec3 p) {
  return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

void main() {
  float scale = aPosScale.w;
  vec3 worldOrigin = aPosScale.xyz;
  float rotation = aTypeParams.y;
  float swayStrength = aTypeParams.z;
  float swaySpeed = aTypeParams.w;
  float swayPhase = aColorSway.a;

  float seed = h31(worldOrigin * 0.173 + vec3(0.27, 0.49, 0.19));

  const float SIZE_MULT = 1.85;
  float sizeJitter = mix(0.90, 1.45, h11(seed * 3.9));
  float finalScale = scale * SIZE_MULT * sizeJitter;

  vec3 localPos = aPos * finalScale;
  float h = clamp(aPos.y, 0.0, 1.0);

  float leanAngle = (h11(seed * 2.1) - 0.5) * 0.18;
  float leanYaw = h11(seed * 3.7) * 6.28318;
  vec2 leanDir = vec2(cos(leanYaw), sin(leanYaw));
  localPos.xz += leanDir * (h * h) * tan(leanAngle) * finalScale;

  float gust = sin(uTime * 0.35 + seed * 6.0) * 0.5 + 0.5;
  float sway = sin(uTime * swaySpeed * uWindSpeed + swayPhase + seed * 4.0);
  sway *= (0.22 + 0.55 * gust) * swayStrength * uWindStrength * pow(h, 1.25);

  float windYaw = seed * 9.0;
  vec2 windDir = normalize(vec2(cos(windYaw), sin(windYaw)) + vec2(0.6, 0.8));
  localPos.xz += windDir * (0.10 * sway);

  float twist = (h11(seed * 5.5) - 0.5) * 0.30;
  float twistAngle = twist * h;
  mat2 tw =
      mat2(cos(twistAngle), -sin(twistAngle), sin(twistAngle), cos(twistAngle));
  localPos.xz = tw * localPos.xz;

  float cs = cos(rotation), sn = sin(rotation);
  mat2 rot = mat2(cs, -sn, sn, cs);
  localPos.xz = rot * localPos.xz;

  vWorldPos = localPos + worldOrigin;

  vec3 n = aNormal;
  n.xz = tw * n.xz;
  n.xz = rot * n.xz;
  vNormal = normalize(n);

  vec3 t = vec3(1.0, 0.0, 0.0);
  vec3 b = vec3(0.0, 0.0, 1.0);
  t.xz = tw * t.xz;
  b.xz = tw * b.xz;
  t.xz = rot * t.xz;
  b.xz = rot * b.xz;

  vTangent = normalize(t);
  vBitangent = normalize(b);

  vHeight = h;
  vSeed = seed;
  vType = aTypeParams.x;
  vColor = aColorSway.rgb;
  vTexCoord = aTexCoord;

  vAlpha = 1.0 - smoothstep(0.49, 0.56, abs(aTexCoord.x - 0.5));

  gl_Position = uViewProj * vec4(vWorldPos, 1.0);
}
