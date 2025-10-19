#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float time;

out vec2 TexCoord;
out vec3 WorldPos;

void main() {
  // Multi-directional wave simulation
  vec3 pos = aPos;

  // Primary waves along river flow
  float wave1 = sin(aPos.z * 0.5 + time * 2.0) * 0.04;
  float wave2 = sin(aPos.x * 0.8 + time * 1.5) * 0.03;
  float wave3 = sin((aPos.x + aPos.z) * 0.3 + time * 2.5) * 0.02;

  // Cross-ripples for realism
  float ripple1 = sin(aPos.x * 2.0 + aPos.z * 1.5 + time * 3.0) * 0.015;
  float ripple2 = cos(aPos.z * 2.5 - aPos.x * 1.2 + time * 2.2) * 0.012;

  // Combine all wave motions
  pos.y += wave1 + wave2 + wave3 + ripple1 + ripple2;

  // Output world position for fragment shader
  WorldPos = (model * vec4(pos, 1.0)).xyz;

  gl_Position = projection * view * model * vec4(pos, 1.0);
  TexCoord = aTexCoord;
}
