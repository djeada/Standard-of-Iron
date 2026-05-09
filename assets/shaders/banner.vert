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
out float v_waveOffset;
out float v_clothDepth;

void main() {
  vec3 pos = a_position;

  float horizontalPos = clamp(a_texCoord.x, 0.0, 1.0);
  float verticalPos = clamp(1.0 - a_texCoord.y, 0.0, 1.0);
  float freeEdge = horizontalPos * horizontalPos;
  float centerBias = 1.0 - abs(verticalPos * 2.0 - 1.0);

  float wavePhase = u_time * 2.3 + horizontalPos * 4.8 - verticalPos * 1.6;
  float zOffset = sin(wavePhase) * u_windStrength * freeEdge *
                  (0.18 + centerBias * 0.08);

  float ripplePhase = u_time * 5.1 + horizontalPos * 10.5 + verticalPos * 6.0;
  float ripple = sin(ripplePhase) * u_windStrength * freeEdge * 0.045;

  float curlPhase = u_time * 7.8 + verticalPos * 3.5 + horizontalPos * 13.0;
  float trailingCurl =
      sin(curlPhase) * u_windStrength * freeEdge * freeEdge * 0.03;

  float swayPhase = u_time * 1.6 + horizontalPos * 1.4 + verticalPos * 2.4;
  float yOffset = sin(swayPhase) * u_windStrength * freeEdge * 0.04;
  float sag = freeEdge * (0.014 + (1.0 - verticalPos) * 0.02) * u_windStrength;

  pos.z += zOffset + ripple + trailingCurl;
  pos.y += yOffset - sag;
  pos.x -= freeEdge * u_windStrength * 0.035;

  vec3 normal = a_normal;
  normal.z += cos(wavePhase) * u_windStrength * freeEdge * 0.45;
  normal.x -= sin(ripplePhase) * u_windStrength * freeEdge * 0.12;
  normal = normalize(normal);

  v_normal = mat3(transpose(inverse(u_model))) * normal;
  v_texCoord = a_texCoord;
  v_waveOffset = zOffset + ripple;
  v_clothDepth = horizontalPos;

  gl_Position = u_mvp * vec4(pos, 1.0);
}
