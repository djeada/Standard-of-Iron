#version 330 core

#ifndef INSTANCED_BATCH_SIZE
#define INSTANCED_BATCH_SIZE 16
#endif

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_tex;
layout(location = 3) in ivec4 a_bone_indices;
layout(location = 4) in vec4 a_bone_weights;

layout(location = 5) in vec4 i_world_c0;
layout(location = 6) in vec4 i_world_c1;
layout(location = 7) in vec4 i_world_c2;
layout(location = 8) in vec4 i_world_c3;
layout(location = 9) in vec4 i_color_alpha;
layout(location = 10) in vec4 i_variation_material;

uniform mat4 u_view_proj;

layout(std140) uniform BonePalette { mat4 bones[INSTANCED_BATCH_SIZE * 64]; }
u_palette;

out vec3 v_normal_ws;
out vec2 v_tex;
out vec3 v_pos_ws;
flat out vec3 v_color;
flat out float v_alpha;
flat out int v_material_id;

void main() {
  int base = gl_InstanceID * 64;
  mat4 skin = a_bone_weights.x * u_palette.bones[base + a_bone_indices.x] +
              a_bone_weights.y * u_palette.bones[base + a_bone_indices.y] +
              a_bone_weights.z * u_palette.bones[base + a_bone_indices.z] +
              a_bone_weights.w * u_palette.bones[base + a_bone_indices.w];

  float wsum =
      a_bone_weights.x + a_bone_weights.y + a_bone_weights.z + a_bone_weights.w;
  if (wsum < 0.001) {
    skin = mat4(1.0);
  }

  mat4 i_world = mat4(i_world_c0, i_world_c1, i_world_c2, i_world_c3);

  vec3 variation_scale = i_variation_material.xyz;
  vec4 pos_local = vec4(a_position * variation_scale, 1.0);
  vec4 pos_world = i_world * skin * pos_local;
  gl_Position = u_view_proj * pos_world;

  mat3 skin_rot = mat3(skin);
  mat3 model_rot = mat3(i_world);
  v_normal_ws = normalize(model_rot * skin_rot * a_normal);
  v_pos_ws = pos_world.xyz;
  v_tex = a_tex;
  v_color = i_color_alpha.rgb;
  v_alpha = i_color_alpha.a;
  v_material_id = int(i_variation_material.w);
}
