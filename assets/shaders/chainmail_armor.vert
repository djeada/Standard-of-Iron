#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

uniform mat4 u_mvp;
uniform mat4 u_model;
uniform mat3 u_normalMatrix;
uniform float u_time; // For dynamic ring shimmer

out vec3 v_normal;
out vec3 v_worldPos;
out vec3 v_viewDir;
out vec2 v_texCoord;
out float v_ringPhase; // For chainmail ring pattern

void main() {
  vec4 worldPos = u_model * vec4(a_position, 1.0);
  v_worldPos = worldPos.xyz;
  v_normal = normalize(u_normalMatrix * a_normal);
  v_texCoord = a_texCoord;

  // Camera position
  vec3 cameraPos = vec3(0.0, 1.5, 5.0);
  v_viewDir = normalize(cameraPos - v_worldPos);

  // Ring phase for interlocking pattern
  // Creates alternating offset rows for realistic chainmail
  float rowIndex = floor(v_worldPos.y * 50.0);
  v_ringPhase = mod(rowIndex, 2.0);

  gl_Position = u_mvp * vec4(a_position, 1.0);
}
