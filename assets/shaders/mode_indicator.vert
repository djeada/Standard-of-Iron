#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

uniform mat4 u_mvp;
uniform mat4 u_model;
uniform float u_time;

out vec3 v_worldPos;
out vec3 v_normal;
out vec2 v_texCoord;
out float v_distFromCenter;

void main() {

  vec3 pos = a_position;
  float pulse = 1.0 + 0.03 * sin(u_time * 3.0);
  pos *= pulse;

  v_distFromCenter = length(a_position.xy);
  v_worldPos = vec3(u_model * vec4(pos, 1.0));
  v_normal = normalize(mat3(u_model) * a_normal);
  v_texCoord = a_texCoord;

  gl_Position = u_mvp * vec4(pos, 1.0);
}
