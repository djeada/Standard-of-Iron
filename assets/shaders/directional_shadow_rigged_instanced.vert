#version 330 core

#ifndef INSTANCED_BATCH_SIZE
#define INSTANCED_BATCH_SIZE 16
#endif

layout(location = 0) in vec3 a_position;
layout(location = 3) in ivec4 a_bone_indices;
layout(location = 4) in vec4 a_bone_weights;
layout(location = 6) in vec4 i_world_c0;
layout(location = 7) in vec4 i_world_c1;
layout(location = 8) in vec4 i_world_c2;
layout(location = 9) in vec4 i_world_c3;
layout(location = 11) in vec4 i_variation_material;

uniform mat4 u_light_vp;

layout(std140) uniform BonePalette {
  mat4 bones[INSTANCED_BATCH_SIZE * 64];
}
u_palette;

void main() {
  int base = gl_InstanceID * 64;
  mat4 skin = a_bone_weights.x * u_palette.bones[base + a_bone_indices.x] +
              a_bone_weights.y * u_palette.bones[base + a_bone_indices.y] +
              a_bone_weights.z * u_palette.bones[base + a_bone_indices.z] +
              a_bone_weights.w * u_palette.bones[base + a_bone_indices.w];
  if (dot(a_bone_weights, vec4(1.0)) < 0.001) {
    skin = mat4(1.0);
  }

  mat4 world = mat4(i_world_c0, i_world_c1, i_world_c2, i_world_c3);
  vec4 local = vec4(a_position * i_variation_material.xyz, 1.0);
  gl_Position = u_light_vp * world * skin * local;
}
