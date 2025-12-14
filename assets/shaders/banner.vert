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

  float horizontalPos = pos.x + 0.5;

  float verticalPos = clamp(0.5 - pos.y, 0.0, 1.0);

  float wavePhase = u_time * 3.0 + horizontalPos * 4.0 + verticalPos * 2.0;
  float waveAmplitude = u_windStrength * horizontalPos * horizontalPos * 0.25;
  float zOffset = sin(wavePhase) * waveAmplitude;

  float ripplePhase = u_time * 4.5 + horizontalPos * 6.0 + verticalPos * 3.0;
  float ripple = sin(ripplePhase) * u_windStrength * horizontalPos * 0.08;

  float flutterPhase = u_time * 8.0 + horizontalPos * 10.0;
  float flutter =
      sin(flutterPhase) * u_windStrength * horizontalPos * horizontalPos * 0.04;

  float swayPhase = u_time * 1.5;
  float yOffset = sin(swayPhase + horizontalPos * 0.8) * u_windStrength *
                  horizontalPos * 0.02;

  pos.z += zOffset + ripple + flutter;
  pos.y += yOffset;

  pos.y -= horizontalPos * horizontalPos * 0.015 * u_windStrength;

  vec3 normal = a_normal;
  float normalWave = cos(wavePhase) * waveAmplitude * 3.0;
  normal.z += normalWave;
  normal = normalize(normal);

  v_normal = mat3(transpose(inverse(u_model))) * normal;
  v_texCoord = a_texCoord;
  v_worldPos = vec3(u_model * vec4(pos, 1.0));
  v_waveOffset = zOffset + ripple;
  v_clothDepth = horizontalPos;

  gl_Position = u_mvp * vec4(pos, 1.0);
}
