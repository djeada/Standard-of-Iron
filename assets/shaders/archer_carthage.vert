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
uniform float u_time;
uniform int u_materialId;

out vec3 v_normal;
out vec3 v_worldNormal;
out vec3 v_tangent;
out vec3 v_bitangent;
out vec2 v_texCoord;
out vec3 v_worldPos;
out vec3 v_instanceColor;
out float v_instanceAlpha;
out float v_armorLayer;
out float v_leatherTension;
out float v_bodyHeight;

float hash13(vec3 p) {
  return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
}

vec3 fallbackUp(vec3 normal) {
  return (abs(normal.y) > 0.92) ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
}

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

  if (u_materialId == 5) {
    float v_raw = a_texCoord.y;
    float v = 1.0 - v_raw;
    float u = a_texCoord.x;

    float pin = smoothstep(0.85, 1.0, v);
    float move = 1.0 - pin;

    float x_norm = (u - 0.5) * 2.0;

    float top_blend = smoothstep(0.92, 1.0, v);
    float edge_emphasis = abs(x_norm);
    float shoulder_wrap = top_blend * (0.45 + edge_emphasis * 0.55);

    pos.y -= shoulder_wrap * 0.08;
    pos.z += shoulder_wrap * 0.08;

    float v_bottom = 1.0 - v;
    float bottom_blend = smoothstep(0.0, 0.35, v_bottom);
    pos.y += bottom_blend * 0.08;

    pos.x += sign(pos.x) * bottom_blend * 0.10;

    float t = u_time * 1.8;
    float wave = sin(pos.z * 5.0 - t + x_norm * 0.5) * 0.02;
    pos.y += wave * move;
  }

  if (u_materialId == 6) {
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

    float t = u_time * 2.0;
    float flutter = sin(t + x_norm * 3.0 + z_norm * 2.0) * 0.005;
    pos.y += flutter;
  }

  mat3 normalMatrix = mat3(eff_model);
  vec3 worldNormal = normalize(normalMatrix * a_normal);

  vec3 t = normalize(cross(fallbackUp(worldNormal), worldNormal));
  if (length(t) < 1e-4)
    t = vec3(1.0, 0.0, 0.0);

  t = normalize(t - worldNormal * dot(worldNormal, t));
  vec3 b = normalize(cross(worldNormal, t));

  vec4 modelPos = eff_model * vec4(pos, 1.0);
  vec3 worldPos = modelPos.xyz;

  vec3 offsetPos = worldPos + worldNormal * 0.008;

  gl_Position = eff_mvp * vec4(pos, 1.0);

  v_worldPos = offsetPos;
  v_texCoord = a_texCoord;
  v_normal = worldNormal;
  v_worldNormal = worldNormal;
  v_tangent = t;
  v_bitangent = b;

  vec3 axisY = vec3(eff_model[1].xyz);
  float axisLen = max(length(axisY), 1e-4);
  vec3 axisDir = axisY / axisLen;
  vec3 modelOrigin = vec3(eff_model[3].xyz);
  float height01 =
      clamp(dot(worldPos - modelOrigin, axisDir) / axisLen + 0.5, 0.0, 1.0);

  float layer_mask = (u_materialId == 1) ? 1.0 : 0.0;
  v_armorLayer = layer_mask;

  float tensionSeed = hash13(offsetPos * 0.35 + worldNormal);
  float heightFactor = height01;
  float curvatureFactor = length(vec2(worldNormal.x, worldNormal.z));
  v_leatherTension = mix(tensionSeed, 1.0 - tensionSeed, layer_mask * 0.33) *
                     (0.7 + curvatureFactor * 0.3) * (0.8 + heightFactor * 0.2);

  v_bodyHeight = height01;
}
