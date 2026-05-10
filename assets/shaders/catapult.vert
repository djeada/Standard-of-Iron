#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

uniform mat4 u_mvp;
uniform mat4 u_model;
layout(std140) uniform FrameData { mat4 u_viewProj; };

out vec3 v_normal;
out vec2 v_texCoord;
out vec3 v_worldPos;
out float v_materialRegion;

void main() {
  vec4 worldPos4 = u_model * vec4(a_position, 1.0);
  gl_Position = u_viewProj * worldPos4;
  v_normal = mat3(u_model) * a_normal;
  v_texCoord = a_texCoord;
  v_worldPos = worldPos4.xyz;

  if (v_worldPos.y < 0.25) {
    v_materialRegion = 0.0;
  } else if (v_worldPos.y < 0.55) {
    v_materialRegion = 1.0;
  } else {
    v_materialRegion = 2.0;
  }
}
