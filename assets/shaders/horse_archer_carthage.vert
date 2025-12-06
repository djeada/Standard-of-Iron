#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

uniform mat4 u_mvp;
uniform mat4 u_model;
uniform int u_materialId;
uniform float u_time;

out vec3 v_normal;
out vec2 v_texCoord;
out vec3 v_worldPos;
out float
    v_armorLayer; // Distinguish armor pieces for Carthaginian Numidian cavalry

void main() {
  vec3 pos = a_position;

  // Cloak back drape (Material ID 12)
  if (u_materialId == 12) {
    float v = 1.0 - a_texCoord.y; // 1 = top, 0 = bottom
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

  // Cloak shoulder cape (Material ID 13)
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

  v_normal = mat3(transpose(inverse(u_model))) * a_normal;
  v_texCoord = a_texCoord;
  vec4 model_pos = u_model * vec4(pos, 1.0);
  v_worldPos = model_pos.xyz;

  // Detect armor layer based on Y position for Carthaginian Numidian cavalry
  // Upper body (helmet) = 0, Torso (light armor/cloak) = 1, Lower (bare
  // legs/horse) = 2
  if (v_worldPos.y > 1.5) {
    v_armorLayer = 0.0; // Bronze cap/no helmet region
  } else if (v_worldPos.y > 0.8) {
    v_armorLayer = 1.0; // Light tunic/imported mail region
  } else {
    v_armorLayer = 2.0; // Bare legs/simple saddle blanket region
  }

  gl_Position = u_mvp * vec4(pos, 1.0);
}
