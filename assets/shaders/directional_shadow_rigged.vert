#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 3) in ivec4 a_bone_indices;
layout(location = 4) in vec4 a_bone_weights;
uniform mat4 u_light_vp;
uniform mat4 u_model;
uniform vec3 u_variation_scale;
layout(std140) uniform BonePalette {
  mat4 bones[64];
}
u_palette;
void main() {
  mat4 skin = a_bone_weights.x * u_palette.bones[a_bone_indices.x] +
              a_bone_weights.y * u_palette.bones[a_bone_indices.y] +
              a_bone_weights.z * u_palette.bones[a_bone_indices.z] +
              a_bone_weights.w * u_palette.bones[a_bone_indices.w];
  float weight_sum = dot(a_bone_weights, vec4(1.0));
  if (weight_sum < 0.001)
    skin = mat4(1.0);
  gl_Position = u_light_vp * u_model * skin * vec4(a_position * u_variation_scale, 1.0);
}
