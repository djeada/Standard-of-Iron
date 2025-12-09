#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float time;

out vec2 TexCoord;
out vec3 WorldPos;
out vec3 Normal;

void main() {
  vec3 pos = aPos;

  float sway = sin(aPos.x * 0.3 + time * 0.5) * 0.005;
  pos.y += sway;

  WorldPos = (model * vec4(pos, 1.0)).xyz;
  Normal = normalize(mat3(transpose(inverse(model))) * aNormal);

  gl_Position = projection * view * model * vec4(pos, 1.0);
  TexCoord = aTexCoord;
}
