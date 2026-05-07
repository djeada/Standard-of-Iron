#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

layout(location = 3) in vec4 a_instanceModelCol0;
layout(location = 4) in vec4 a_instanceModelCol1;
layout(location = 5) in vec4 a_instanceModelCol2;
layout(location = 6) in vec4 a_instanceColorAlpha;

uniform mat4 u_viewProj;

out vec3 v_normal;
out vec2 v_texCoord;
out vec3 v_worldPos;
flat out vec3 v_instanceColor;
flat out float v_instanceAlpha;

void main() {
  mat4 model = mat4(vec4(a_instanceModelCol0.xyz, 0.0),
                    vec4(a_instanceModelCol1.xyz, 0.0),
                    vec4(a_instanceModelCol2.xyz, 0.0),
                    vec4(a_instanceModelCol0.w, a_instanceModelCol1.w,
                         a_instanceModelCol2.w, 1.0));

  vec4 worldPos4 = model * vec4(a_position, 1.0);
  v_worldPos = worldPos4.xyz;
  v_normal = mat3(model) * a_normal;
  v_texCoord = a_texCoord;
  v_instanceColor = a_instanceColorAlpha.rgb;
  v_instanceAlpha = a_instanceColorAlpha.a;
  gl_Position = u_viewProj * worldPos4;
}
