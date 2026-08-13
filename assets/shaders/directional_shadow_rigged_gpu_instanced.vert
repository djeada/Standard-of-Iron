#version 430 core

#include "rigged_gpu_skin.glsl"

uniform int u_rigid_skinning;

void main() {
  uint v = uint(gl_VertexID);
  uint instance = uint(gl_InstanceID);
  gl_Position = u_rigid_skinning != 0 ? rg_clip_position_rigid(v, instance)
                                      : rg_clip_position(v, instance);
}
