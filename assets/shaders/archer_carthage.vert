#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

uniform mat4 u_mvp;
uniform mat4 u_model;

out vec3 v_normal;
out vec2 v_texCoord;
out vec3 v_worldPos;
out float
    v_armorLayer; // NEW: Distinguish armor pieces for Carthaginian light armor

void main() {
  v_normal = mat3(transpose(inverse(u_model))) * a_normal;
  v_texCoord = a_texCoord;
  v_worldPos = vec3(u_model * vec4(a_position, 1.0));

  // Detect armor layer based on Y position for lighter Carthaginian equipment
  // Upper body (helmet/head) = 0, Torso = 1, Lower = 2
  if (v_worldPos.y > 1.5) {
    v_armorLayer = 0.0; // Leather cap/light helmet region
  } else if (v_worldPos.y > 0.8) {
    v_armorLayer = 1.0; // Linothorax (linen) torso region
  } else {
    v_armorLayer = 2.0; // Leather skirt/pteruges region
  }

  gl_Position = u_mvp * vec4(a_position, 1.0);
}
