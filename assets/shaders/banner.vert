#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

uniform mat4 u_mvp;
uniform mat4 u_model;
uniform float u_time;
uniform float u_windStrength;

out vec3 v_normal;
out vec2 v_texCoord;
out vec3 v_worldPos;
out float v_waveOffset;
out float v_clothDepth;

float hash(float n) { return fract(sin(n) * 43758.5453123); }

void main() {
  vec3 pos = a_position;

  float verticalPos = clamp(0.5 - pos.y, 0.0, 1.0);

  float horizontalPos = pos.x + 0.5;

  float wavePhase = u_time * 2.5 + verticalPos * 6.28318 + horizontalPos * 2.0;
  float waveAmplitude = u_windStrength * verticalPos * verticalPos * 0.15;
  float zOffset = sin(wavePhase) * waveAmplitude;

  float ripplePhase = u_time * 3.7 + verticalPos * 4.0 + horizontalPos * 3.0;
  float ripple = sin(ripplePhase) * u_windStrength * verticalPos * 0.05;

  float swayPhase = u_time * 1.2;
  float xOffset =
      sin(swayPhase + verticalPos * 0.5) * u_windStrength * verticalPos * 0.03;

  pos.z += zOffset + ripple;
  pos.x += xOffset;

  pos.y -= verticalPos * verticalPos * 0.02 * u_windStrength;

  vec3 normal = a_normal;
  float normalWave = cos(wavePhase) * waveAmplitude * 2.0;
  normal.z += normalWave;
  normal = normalize(normal);

  v_normal = mat3(transpose(inverse(u_model))) * normal;
  v_texCoord = a_texCoord;
  v_worldPos = vec3(u_model * vec4(pos, 1.0));
  v_waveOffset = zOffset;
  v_clothDepth = verticalPos;

  gl_Position = u_mvp * vec4(pos, 1.0);
}
