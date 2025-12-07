#version 330 core

// ============================================================================
// CARTHAGINIAN HEALER VERTEX SHADER
// Flowing Mediterranean robes with natural draping
// ============================================================================

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
out float v_clothFolds;
out float v_fabricWear;

float hash13(vec3 p) {
  return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
}

vec3 fallbackUp(vec3 n) {
  return (abs(n.y) > 0.92) ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
}

void main() {
  vec3 position = a_position;
  vec3 normal = a_normal;

  mat3 normalMatrix = mat3(transpose(inverse(u_model)));
  vec3 worldNormal = normalize(normalMatrix * normal);

  // Build tangent space for detailed shading
  vec3 t = normalize(cross(fallbackUp(worldNormal), worldNormal));
  if (length(t) < 1e-4)
    t = vec3(1.0, 0.0, 0.0);
  t = normalize(t - worldNormal * dot(worldNormal, t));
  vec3 b = normalize(cross(worldNormal, t));

  v_normal = normal;
  v_worldNormal = worldNormal;
  v_tangent = t;
  v_bitangent = b;
  v_texCoord = a_texCoord;
  v_worldPos = vec3(u_model * vec4(position, 1.0));

  // Body height for cloth flow (0.0 = feet, 1.0 = head)
  v_bodyHeight = clamp((v_worldPos.y + 0.2) / 1.8, 0.0, 1.0);

  // Phoenician robes drape differently - looser, more flowing folds
  // Emphasis on chest/waist gather and lower hem flow
  float chestGather = smoothstep(1.05, 1.20, v_worldPos.y) *
                      smoothstep(1.35, 1.20, v_worldPos.y);
  float waistSash = smoothstep(0.80, 0.92, v_worldPos.y) *
                    smoothstep(1.04, 0.92, v_worldPos.y);
  float hemFlow = smoothstep(0.50, 0.35, v_worldPos.y) *
                  smoothstep(0.15, 0.35, v_worldPos.y);
  float fold_wave = sin(v_worldPos.x * 3.6 + v_worldPos.z * 4.1) * 0.10 +
                    sin(v_worldPos.x * 6.2 - v_worldPos.z * 3.1) * 0.06;
  v_clothFolds = clamp((chestGather * 0.6 + waistSash * 0.8 + hemFlow * 0.5) +
                           fold_wave * 0.35,
                       0.0, 1.2);

  // Fabric wear pattern - more at edges and stress points
  float shoulderStress = smoothstep(1.05, 1.45, v_worldPos.y) * 0.35;
  float hemWear = smoothstep(0.55, 0.15, v_worldPos.y) * 0.45;
  v_fabricWear =
      hash13(v_worldPos * 0.4) * 0.22 + 0.14 + shoulderStress + hemWear;

  // Keep material selection stable; armor layer only toggles for armor.
  v_armorLayer = (u_materialId == 1) ? 1.0 : 0.0;

  gl_Position = u_mvp * vec4(position, 1.0);
}
