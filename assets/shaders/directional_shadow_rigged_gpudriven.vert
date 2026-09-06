#version 430 core

#include "rigged_gpu_skin.glsl"

uniform int u_rigid_skinning;

void main() {
  uint gid = uint(gl_VertexID);
  uint instance = gid / u_vertex_count;
  uint v = gid - instance * u_vertex_count;
  gl_Position = u_rigid_skinning != 0 ? rg_clip_position_rigid(v, instance)
                                      : rg_clip_position(v, instance);
}
