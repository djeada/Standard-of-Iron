#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

layout(location = 3) in vec4 i_modelCol0;
layout(location = 4) in vec4 i_modelCol1;
layout(location = 5) in vec4 i_modelCol2;
layout(location = 6) in vec4 i_colorAlpha;

uniform mat4 u_viewProj;
uniform vec3 u_lightDir;

out vec3 v_worldPos;
out vec3 v_normal;
out vec3 v_color;
out float v_alpha;

void main() {

  mat4 modelMatrix =
      mat4(vec4(i_modelCol0.xyz, 0.0), vec4(i_modelCol1.xyz, 0.0),
           vec4(i_modelCol2.xyz, 0.0),
           vec4(i_modelCol0.w, i_modelCol1.w, i_modelCol2.w, 1.0));

  vec4 worldPos4 = modelMatrix * vec4(a_position, 1.0);
  v_worldPos = worldPos4.xyz;

  mat3 normalMatrix = mat3(modelMatrix);

  v_normal = normalize(normalMatrix * a_normal);

  v_color = i_colorAlpha.rgb;
  v_alpha = i_colorAlpha.a;

  gl_Position = u_viewProj * worldPos4;
}
