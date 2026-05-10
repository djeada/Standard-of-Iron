#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

layout(location = 3) in vec4 a_instanceModelCol0;
layout(location = 4) in vec4 a_instanceModelCol1;
layout(location = 5) in vec4 a_instanceModelCol2;
layout(location = 6) in vec4 a_instanceColorAlpha;

layout(std140) uniform FrameData { mat4 u_viewProj; };

uniform float u_time;

flat out vec3 v_instanceColor;
flat out float v_instanceAlpha;
out float v_distFromCenter;

void main() {
  mat4 model = mat4(vec4(a_instanceModelCol0.xyz, 0.0),
                    vec4(a_instanceModelCol1.xyz, 0.0),
                    vec4(a_instanceModelCol2.xyz, 0.0),
                    vec4(a_instanceModelCol0.w, a_instanceModelCol1.w,
                         a_instanceModelCol2.w, 1.0));

  vec3 pos = a_position;
  float pulse = 1.0 + 0.03 * sin(u_time * 3.0);
  pos *= pulse;

  v_distFromCenter = length(a_position.xy);
  v_instanceColor = a_instanceColorAlpha.rgb;
  v_instanceAlpha = a_instanceColorAlpha.a;

  gl_Position = u_viewProj * model * vec4(pos, 1.0);
}
