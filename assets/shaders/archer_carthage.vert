#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

uniform mat4 u_mvp;
uniform mat4 u_model;
uniform float u_time;
uniform int u_materialId;

out vec3 v_normal;
out vec3 v_worldNormal;
out vec3 v_tangent;
out vec3 v_bitangent;
out vec2 v_texCoord;
out vec3 v_worldPos;
out float v_armorLayer;
out float v_leatherTension;
out float v_bodyHeight;

float hash13(vec3 p) {
  return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
}

vec3 fallbackUp(vec3 normal) {
  return (abs(normal.y) > 0.92) ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
}

void main() {
  vec3 pos = a_position;

  // ==========================================================
  // CLOAK BACK DRAPE (Material ID 5) - Vertical, hangs down back
  // pos.x = left/right, pos.z = length (top to bottom in world)
  // ==========================================================
  if (u_materialId == 5) {
    float v_raw = a_texCoord.y; // mesh uses 0 at top, 1 at bottom
    float v = 1.0 - v_raw;      // normalize so 1 = top, 0 = bottom
    float u = a_texCoord.x;     // 0 left, 1 right

    // Pin top portion
    // Keep only the very top firmly attached
    float pin = smoothstep(0.85, 1.0, v);
    float move = 1.0 - pin;

    float x_norm = (u - 0.5) * 2.0; // -1 to +1 left/right

    // Top folds onto the shoulders: only affect the upper band so the body of
    // the cloak hangs straight
    float top_blend = smoothstep(0.92, 1.0, v);
    float edge_emphasis = abs(x_norm);
    float shoulder_wrap = top_blend * (0.45 + edge_emphasis * 0.55);
    // Pull toward the body and fold the top edge downward (increase z)
    pos.y -= shoulder_wrap * 0.08;
    pos.z += shoulder_wrap * 0.08;

    // Bottom flares outward (positive Y = away from body)
    float v_bottom = 1.0 - v;
    float bottom_blend = smoothstep(0.0, 0.35, v_bottom);
    pos.y += bottom_blend * 0.08;

    // Bottom is slightly wider - expand X at bottom
    pos.x += sign(pos.x) * bottom_blend * 0.10;

    // Gentle wind animation
    float t = u_time * 1.8;
    float wave = sin(pos.z * 5.0 - t + x_norm * 0.5) * 0.02;
    pos.y += wave * move;
  }

  // ==========================================================
  // CLOAK SHOULDER CAPE (Material ID 6) - Drapes over shoulders
  // Plane mesh is in XZ (flat), Y is up. pos.x = side, pos.z = front-back
  // ==========================================================
  if (u_materialId == 6) {
    float u = a_texCoord.x; // 0 to 1 left-right
    float v = a_texCoord.y; // 0 front, 1 back

    // Normalized coordinates
    float x_norm = (u - 0.5) * 2.0; // -1 left, +1 right
    float x_abs = abs(x_norm);
    float z_norm = (v - 0.5) * 2.0; // -1 front, +1 back

    // === SHOULDER DROOP ===
    // Edges droop down gently to follow shoulder slope
    float shoulder_droop = x_abs * x_abs * 0.10;
    pos.y -= shoulder_droop;

    // === FRONT-BACK CURVE ===
    // Back droops slightly to connect with back drape
    float back_droop = max(0.0, z_norm) * max(0.0, z_norm) * 0.06;
    pos.y -= back_droop;

    // Front edge curves down slightly
    float front_droop = max(0.0, -z_norm) * max(0.0, -z_norm) * 0.03;
    pos.y -= front_droop;

    // Subtle animated flutter
    float t = u_time * 2.0;
    float flutter = sin(t + x_norm * 3.0 + z_norm * 2.0) * 0.005;
    pos.y += flutter;
  }

  mat3 normalMatrix = mat3(transpose(inverse(u_model)));
  vec3 worldNormal = normalize(normalMatrix * a_normal);

  // Build a stable TBN without needing extra attributes
  vec3 t = normalize(cross(fallbackUp(worldNormal), worldNormal));
  if (length(t) < 1e-4)
    t = vec3(1.0, 0.0, 0.0);
  // Gram–Schmidt to ensure orthonormality
  t = normalize(t - worldNormal * dot(worldNormal, t));
  vec3 b = normalize(cross(worldNormal, t));

  vec4 modelPos = u_model * vec4(pos, 1.0);
  vec3 worldPos = modelPos.xyz;

  // Small normal push to avoid self-shadow acne in forward pipelines
  vec3 offsetPos = worldPos + worldNormal * 0.008;

  gl_Position = u_mvp * vec4(pos, 1.0);

  v_worldPos = offsetPos;
  v_texCoord = a_texCoord;
  v_normal = worldNormal;
  v_worldNormal = worldNormal;
  v_tangent = t;
  v_bitangent = b;

  // Layer bands (kept identical thresholds to preserve your gameplay logic)
  float height = offsetPos.y;
  float layer = 2.0;
  if (height > 1.28)
    layer = 0.0;
  else if (height > 0.86)
    layer = 1.0;
  v_armorLayer = layer;

  // Leather tension: variation + curvature bias + height influence
  float tensionSeed = hash13(offsetPos * 0.35 + worldNormal);
  float heightFactor = smoothstep(0.5, 1.5, height);
  float curvatureFactor = length(vec2(worldNormal.x, worldNormal.z));
  v_leatherTension = mix(tensionSeed, 1.0 - tensionSeed, layer * 0.33) *
                     (0.7 + curvatureFactor * 0.3) * (0.8 + heightFactor * 0.2);

  // Normalized torso height for gradient effects
  float torsoMin = 0.58;
  float torsoMax = 1.36;
  v_bodyHeight =
      clamp((offsetPos.y - torsoMin) / (torsoMax - torsoMin), 0.0, 1.0);
}
