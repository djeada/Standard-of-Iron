#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_tex;
layout(location = 3) in ivec4 a_bone_indices;
layout(location = 4) in vec4 a_bone_weights;

uniform mat4 u_view_proj;
uniform mat4 u_model;
uniform vec3 u_variation_scale;

layout(std140) uniform BonePalette { mat4 bones[64]; }
u_palette;

out vec3 v_normal_ws;
out vec2 v_tex;
out vec3 v_pos_ws;

void main() {
  mat4 skin = a_bone_weights.x * u_palette.bones[a_bone_indices.x] +
              a_bone_weights.y * u_palette.bones[a_bone_indices.y] +
              a_bone_weights.z * u_palette.bones[a_bone_indices.z] +
              a_bone_weights.w * u_palette.bones[a_bone_indices.w];

  float wsum =
      a_bone_weights.x + a_bone_weights.y + a_bone_weights.z + a_bone_weights.w;
  if (wsum < 0.001) {
    skin = mat4(1.0);
  }

  vec4 pos_local = vec4(a_position * u_variation_scale, 1.0);
  vec4 pos_world = u_model * skin * pos_local;
  gl_Position = u_view_proj * pos_world;

  mat3 skin_rot = mat3(skin);
  mat3 model_rot = mat3(u_model);
  v_normal_ws = normalize(model_rot * skin_rot * a_normal);
  v_pos_ws = pos_world.xyz;
  v_tex = a_tex;
}
