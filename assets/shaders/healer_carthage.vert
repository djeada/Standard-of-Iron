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
  v_clothFolds = (chestGather * 0.6 + waistSash * 0.8 + hemFlow * 0.5);

  // Fabric wear pattern - more at edges and stress points
  v_fabricWear = hash13(v_worldPos * 0.4) * 0.25 + 0.15;

  // Legacy armor layer (repurposed for material regions)
  if (v_worldPos.y > 1.5) {
    v_armorLayer = 0.0; // Head/face region (for beard)
  } else if (v_worldPos.y > 0.8) {
    v_armorLayer = 1.0; // Torso/robe region
  } else {
    v_armorLayer = 2.0; // Lower body/legs
  }

  gl_Position = u_mvp * vec4(position, 1.0);
}
