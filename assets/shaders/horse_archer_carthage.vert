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
uniform float u_time;

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
  vec3 pos = a_position;

  if (u_materialId == 12) {
    float v = 1.0 - a_texCoord.y;
    float u = a_texCoord.x;
    float x_norm = (u - 0.5) * 2.0;

    float pin = smoothstep(0.85, 1.0, v);
    float move = 1.0 - pin;

    float top_blend = smoothstep(0.90, 1.0, v);
    float edge_emphasis = abs(x_norm);
    float shoulder_wrap = top_blend * (0.42 + edge_emphasis * 0.55);
    pos.y -= shoulder_wrap * 0.08;
    pos.z += shoulder_wrap * 0.08;

    float bottom_blend = smoothstep(0.0, 0.30, 1.0 - v);
    pos.y += bottom_blend * 0.08;
    pos.x += sign(pos.x) * bottom_blend * 0.10;

    float wave = sin(pos.z * 5.0 + x_norm * 0.5 + u_time * 1.6) * 0.02;
    pos.y += wave * move;
  }

  if (u_materialId == 13) {
    float u = a_texCoord.x;
    float v = a_texCoord.y;
    float x_norm = (u - 0.5) * 2.0;
    float x_abs = abs(x_norm);
    float z_norm = (v - 0.5) * 2.0;

    float shoulder_droop = x_abs * x_abs * 0.10;
    pos.y -= shoulder_droop;

    float back_droop = max(0.0, z_norm) * max(0.0, z_norm) * 0.06;
    pos.y -= back_droop;

    float front_droop = max(0.0, -z_norm) * max(0.0, -z_norm) * 0.03;
    pos.y -= front_droop;

    float flutter = sin(u_time * 2.0 + x_norm * 3.0 + z_norm * 2.0) * 0.005;
    pos.y += flutter;
  }

  v_normal = mat3(eff_model) * a_normal;
  v_texCoord = a_texCoord;
  vec4 model_pos = eff_model * vec4(pos, 1.0);
  v_worldPos = model_pos.xyz;

  v_armorLayer = (u_materialId == 1) ? 1.0 : 0.0;

  gl_Position = eff_mvp * vec4(pos, 1.0);
}
