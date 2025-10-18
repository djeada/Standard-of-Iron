#version 330 core

// ─────────────────────────────────────────────────────────────
// Vertex Attributes
// ─────────────────────────────────────────────────────────────
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec4 aPosScale;  // instance: xyz = world pos, w = scale
layout(location = 4) in vec4 aColorSway; // instance: rgb = tint, a = sway phase
layout(location =
           5) in vec4 aRotation; // instance: x = Y-axis rotation, yzw = seeds

// ─────────────────────────────────────────────────────────────
// Uniforms
// ─────────────────────────────────────────────────────────────
uniform mat4 uViewProj;
uniform float uTime;
uniform float uWindStrength;
uniform float uWindSpeed;

// ─────────────────────────────────────────────────────────────
// Varyings
// ─────────────────────────────────────────────────────────────
out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;
out vec2 vTexCoord;
out float vFoliageMask;
out float vNeedleSeed;
out float vBarkSeed;

// ─────────────────────────────────────────────────────────────
// Main Shader Logic
// ─────────────────────────────────────────────────────────────
void main() {
  const float TWO_PI = 6.2831853;

  // Instance data unpacking
  float scale = aPosScale.w;
  vec3 worldPos = aPosScale.xyz;
  float swayPhase = aColorSway.a;
  float rotation = aRotation.x;
  float silhouetteSeed = aRotation.y;
  float needleSeed = aRotation.z;
  float barkSeed = aRotation.w;

  vec3 modelPos = aPos;

  // ── Foliage and tip region masks ──────────────────────────
  float foliageMask = smoothstep(0.34, 0.42, aTexCoord.y);
  float tipMask = smoothstep(0.88, 1.02, aTexCoord.y);
  float angle = aTexCoord.x * TWO_PI;

  // ── Irregular silhouette shaping ──────────────────────────
  float irregularBase = sin(angle * 3.0 + silhouetteSeed * TWO_PI);
  float irregularFine = sin(angle * 5.0 + silhouetteSeed * TWO_PI * 2.0);
  float irregular = (irregularBase * 0.11 + irregularFine * 0.05) *
                    foliageMask * (1.0 - tipMask * 0.6);

  modelPos.xz *= (1.0 + irregular);

  // Slight droop on foliage
  float droop = foliageMask * (1.0 - tipMask) * 0.08;
  modelPos.y -= droop;

  // Height-based influence
  float heightFactor = clamp(modelPos.y, 0.0, 1.1);
  vec3 localPos = modelPos * scale;

  // ── Wind sway (stronger at upper regions) ─────────────────
  float sway = sin(uTime * uWindSpeed * 0.5 + swayPhase) * uWindStrength * 0.8 *
               heightFactor * heightFactor;

  float swayInfluence = mix(0.04, 0.12, foliageMask);
  localPos.x += sway * swayInfluence;

  // Slight forward bend for branches
  localPos.y -= sway * 0.02 * foliageMask;

  // ── Adjust normals for irregular foliage ──────────────────
  vec3 localNormal = aNormal;
  if (foliageMask > 0.0) {
    float normalScale = 1.0 + irregular;
    localNormal = normalize(vec3(localNormal.x * normalScale,
                                 localNormal.y - foliageMask * 0.2,
                                 localNormal.z * normalScale));
  }

  // ── Instance rotation about Y-axis ────────────────────────
  float cosR = cos(rotation);
  float sinR = sin(rotation);
  mat2 rot = mat2(cosR, -sinR, sinR, cosR);

  vec2 rotatedXZ = rot * localPos.xz;
  localPos = vec3(rotatedXZ.x, localPos.y, rotatedXZ.y);

  vec2 rotatedNormalXZ = rot * localNormal.xz;
  vec3 finalNormal =
      normalize(vec3(rotatedNormalXZ.x, localNormal.y, rotatedNormalXZ.y));

  // ── Outputs ───────────────────────────────────────────────
  vWorldPos = localPos + worldPos;
  vNormal = finalNormal;
  vColor = aColorSway.rgb;
  vTexCoord = aTexCoord;
  vFoliageMask = foliageMask;
  vNeedleSeed = needleSeed;
  vBarkSeed = barkSeed;

  gl_Position = uViewProj * vec4(vWorldPos, 1.0);
}
