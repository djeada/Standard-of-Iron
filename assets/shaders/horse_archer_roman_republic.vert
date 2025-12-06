#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

uniform mat4 u_mvp;
uniform mat4 u_model;
uniform int u_materialId;
uniform float u_time;

out vec3 v_normal;
out vec3 v_worldNormal;
out vec3 v_tangent;
out vec3 v_bitangent;
out vec2 v_texCoord;
out vec3 v_worldPos;
out float v_armorLayer;  // Distinguish armor pieces for Roman equites cavalry
out float v_bodyHeight;  // 0.0-1.0 for both rider and horse
out float v_armorSheen;  // Rider armor polish level
out float v_leatherWear; // Tack aging
out float v_horseMusculature; // Horse body definition
out float v_hairFlow;         // Mane/tail direction
out float v_hoofWear;         // Hoof chipping

float hash13(vec3 p) {
  return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
}

vec3 fallbackUp(vec3 n) {
  return (abs(n.y) > 0.92) ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
}

void main() {
  vec3 position = a_position;
  vec3 normal = a_normal;

  // Cloak back drape (Material ID 12)
  if (u_materialId == 12) {
    float v = 1.0 - a_texCoord.y; // 1 = top, 0 = bottom
    float u = a_texCoord.x;
    float x_norm = (u - 0.5) * 2.0;

    float pin = smoothstep(0.85, 1.0, v);
    float move = 1.0 - pin;

    float top_blend = smoothstep(0.90, 1.0, v);
    float edge_emphasis = abs(x_norm);
    float shoulder_wrap = top_blend * (0.42 + edge_emphasis * 0.55);
    position.y -= shoulder_wrap * 0.08;
    position.z += shoulder_wrap * 0.08;

    float bottom_blend = smoothstep(0.0, 0.30, 1.0 - v);
    position.y += bottom_blend * 0.08;
    position.x += sign(position.x) * bottom_blend * 0.10;

    float wave = sin(position.z * 5.0 + x_norm * 0.5 + u_time * 1.6) * 0.02;
    position.y += wave * move;
  }

  // Cloak shoulder cape (Material ID 13)
  if (u_materialId == 13) {
    float u = a_texCoord.x;
    float v = a_texCoord.y;
    float x_norm = (u - 0.5) * 2.0;
    float x_abs = abs(x_norm);
    float z_norm = (v - 0.5) * 2.0;

    float shoulder_droop = x_abs * x_abs * 0.10;
    position.y -= shoulder_droop;

    float back_droop = max(0.0, z_norm) * max(0.0, z_norm) * 0.06;
    position.y -= back_droop;

    float front_droop = max(0.0, -z_norm) * max(0.0, -z_norm) * 0.03;
    position.y -= front_droop;

    float flutter = sin(u_time * 2.0 + x_norm * 3.0 + z_norm * 2.0) * 0.005;
    position.y += flutter;
  }

  mat3 normalMatrix = mat3(transpose(inverse(u_model)));
  vec3 worldNormal = normalize(normalMatrix * normal);

  // Build tangent space for normal mapping
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

  // Body height for both rider and horse (0.0-1.0)
  v_bodyHeight = clamp((v_worldPos.y + 0.2) / 2.0, 0.0, 1.0);

  // Procedural detail based on world position
  float hashVal = hash13(v_worldPos * 0.5);

  // Rider equipment details
  v_armorSheen = 0.6 + hashVal * 0.3;  // Polish variation
  v_leatherWear = hashVal * 0.4 + 0.1; // Tack wear (0.1-0.5)

  // Horse details
  v_horseMusculature =
      smoothstep(0.3, 0.6, v_bodyHeight) * smoothstep(1.0, 0.7, v_bodyHeight);
  v_hairFlow = hashVal * 0.5 + 0.5;
  v_hoofWear = hashVal * 0.3;

  // Legacy armor layer for fallback
  if (v_worldPos.y > 1.5) {
    v_armorLayer = 0.0; // Helmet region
  } else if (v_worldPos.y > 0.8) {
    v_armorLayer = 1.0; // Torso armor region
  } else {
    v_armorLayer = 2.0; // Legs/horse region
  }

  gl_Position = u_mvp * vec4(position, 1.0);
}
