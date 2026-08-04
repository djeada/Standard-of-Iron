#ifndef RIGGED_GPU_SKIN_GLSL
#define RIGGED_GPU_SKIN_GLSL

struct RiggedInstanceData {
  mat4 world;
  vec4 color_alpha;
  vec4 variation_material;
  vec4 wear_params;
  vec4 role_meta;
};

layout(std430, binding = 0) readonly buffer RiggedVertexData {
  uint rg_vertex_words[];
};

layout(std430, binding = 2) readonly buffer RiggedPaletteData {
  mat4 rg_palette[];
};

layout(std430, binding = 3) readonly buffer RiggedInstanceBuffer {
  RiggedInstanceData rg_instance[];
};

uniform mat4 u_view_proj;
uniform uint u_vertex_count;
uniform uint u_bone_count;

const uint RG_WORDS_PER_VERTEX = 14u;

vec3 rg_position(uint v) {
  uint b = v * RG_WORDS_PER_VERTEX;
  return vec3(uintBitsToFloat(rg_vertex_words[b + 0u]),
              uintBitsToFloat(rg_vertex_words[b + 1u]),
              uintBitsToFloat(rg_vertex_words[b + 2u]));
}

vec3 rg_normal(uint v) {
  uint b = v * RG_WORDS_PER_VERTEX + 3u;
  return vec3(uintBitsToFloat(rg_vertex_words[b + 0u]),
              uintBitsToFloat(rg_vertex_words[b + 1u]),
              uintBitsToFloat(rg_vertex_words[b + 2u]));
}

vec2 rg_texcoord(uint v) {
  uint b = v * RG_WORDS_PER_VERTEX + 6u;
  return vec2(uintBitsToFloat(rg_vertex_words[b + 0u]),
              uintBitsToFloat(rg_vertex_words[b + 1u]));
}

uvec4 rg_bone_indices(uint v) {
  uint bone_bits = rg_vertex_words[v * RG_WORDS_PER_VERTEX + 8u];
  uvec4 indices = uvec4(bone_bits & 0xFFu,
                        (bone_bits >> 8u) & 0xFFu,
                        (bone_bits >> 16u) & 0xFFu,
                        (bone_bits >> 24u) & 0xFFu);
  return min(indices, uvec4(max(u_bone_count, 1u) - 1u));
}

vec4 rg_bone_weights(uint v) {
  uint b = v * RG_WORDS_PER_VERTEX + 9u;
  return vec4(uintBitsToFloat(rg_vertex_words[b + 0u]),
              uintBitsToFloat(rg_vertex_words[b + 1u]),
              uintBitsToFloat(rg_vertex_words[b + 2u]),
              uintBitsToFloat(rg_vertex_words[b + 3u]));
}

uint rg_color_role(uint v) {
  return rg_vertex_words[v * RG_WORDS_PER_VERTEX + 13u] & 0xFFu;
}

mat4 rg_skin_matrix(uint v, uint instance) {
  uvec4 bi = rg_bone_indices(v);
  vec4 bw = rg_bone_weights(v);
  uint base = instance * u_bone_count;
  mat4 skin = bw.x * rg_palette[base + bi.x] + bw.y * rg_palette[base + bi.y] +
              bw.z * rg_palette[base + bi.z] + bw.w * rg_palette[base + bi.w];
  float wsum = bw.x + bw.y + bw.z + bw.w;
  if (wsum < 0.001) {
    skin = mat4(1.0);
  }
  return skin;
}

vec4 rg_clip_position(uint v, uint instance) {
  mat4 skin = rg_skin_matrix(v, instance);
  vec3 variation_scale = rg_instance[instance].variation_material.xyz;
  vec4 pos_local = vec4(rg_position(v) * variation_scale, 1.0);
  vec4 skinned_local = skin * pos_local;
  vec4 pos_world = rg_instance[instance].world * skinned_local;
  return u_view_proj * pos_world;
}

#endif
