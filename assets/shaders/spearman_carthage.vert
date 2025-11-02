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
    v_armorLayer; // Distinguish armor pieces for Carthaginian Libyan spearman

void main() {
  v_normal = mat3(transpose(inverse(u_model))) * a_normal;
  v_texCoord = a_texCoord;
  v_worldPos = vec3(u_model * vec4(a_position, 1.0));

  // Detect armor layer based on Y position for Carthaginian Libyan infantry
  // Upper body (helmet) = 0, Torso (linothorax/captured) = 1, Lower
  // (greaves/skirt) = 2
  if (v_worldPos.y > 1.5) {
    v_armorLayer = 0.0; // Captured helmet/Iberian style region
  } else if (v_worldPos.y > 0.8) {
    v_armorLayer = 1.0; // Linothorax/captured mail region
  } else {
    v_armorLayer = 2.0; // Leather greaves/pteruges region
  }

  gl_Position = u_mvp * vec4(a_position, 1.0);
}
