#version 430 core

#include "rigged_gpu_skin.glsl"

void main() {
  uint gid = uint(gl_VertexID);
  uint instance = gid / u_vertex_count;
  uint v = gid - instance * u_vertex_count;
  gl_Position = rg_clip_position(v, instance);
}
