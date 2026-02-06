#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;
layout(location = 3) in vec4 a_instanceModelCol0;
layout(location = 4) in vec4 a_instanceModelCol1;
layout(location = 5) in vec4 a_instanceModelCol2;
layout(location = 6) in vec4 a_instanceColorAlpha;

uniform mat4 u_mvp;
uniform mat4 u_model;
uniform mat4 u_viewProj;
uniform bool u_instanced;
uniform int u_materialId;

out vec3 v_normal;
out vec2 v_texCoord;
out vec3 v_worldPos;
out vec3 v_instanceColor;
out float v_instanceAlpha;
out float v_armorLayer;

void main() {
  mat4 eff_model;
  mat4 eff_mvp;
  if (u_instanced) {
    eff_model = mat4(vec4(a_instanceModelCol0.xyz, 0.0),
                     vec4(a_instanceModelCol1.xyz, 0.0),
                     vec4(a_instanceModelCol2.xyz, 0.0),
                     vec4(a_instanceModelCol0.w, a_instanceModelCol1.w,
                          a_instanceModelCol2.w, 1.0));
    eff_mvp = u_viewProj * eff_model;
    v_instanceColor = a_instanceColorAlpha.rgb;
    v_instanceAlpha = a_instanceColorAlpha.a;
  } else {
    eff_model = u_model;
    eff_mvp = u_mvp;
    v_instanceColor = vec3(0.0);
    v_instanceAlpha = 1.0;
  }
  v_normal = mat3(eff_model) * a_normal;
  v_texCoord = a_texCoord;
  v_worldPos = vec3(eff_model * vec4(a_position, 1.0));

  v_armorLayer = (u_materialId == 1) ? 1.0 : 0.0;

  gl_Position = eff_mvp * vec4(a_position, 1.0);
}
