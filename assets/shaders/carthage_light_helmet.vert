#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

uniform mat4 u_mvp;
uniform mat4 u_model;
uniform mat3 u_normalMatrix;

out vec3 v_normal;
out vec3 v_worldPos;
out vec3 v_viewDir;
out vec2 v_texCoord;
out float v_metallic;
out float v_roughness;

void main() {
  vec4 worldPos = u_model * vec4(a_position, 1.0);
  v_worldPos = worldPos.xyz;
  v_normal = normalize(u_normalMatrix * a_normal);
  v_texCoord = a_texCoord;

  // Camera at approximate position for view direction
  vec3 cameraPos = vec3(0.0, 1.5, 5.0);
  v_viewDir = normalize(cameraPos - v_worldPos);

  // Material properties based on position
  // Bronze helmet: high metallic, medium roughness
  if (v_worldPos.y > 1.6) {
    v_metallic = 0.85;  // Bronze
    v_roughness = 0.35; // Polished but weathered
  } else if (v_worldPos.y > 1.5) {
    v_metallic = 0.75; // Crest mount
    v_roughness = 0.45;
  } else {
    v_metallic = 0.65; // Cheek guards
    v_roughness = 0.5;
  }

  gl_Position = u_mvp * vec4(a_position, 1.0);
}
