#version 430 core

#include "rigged_gpu_skin.glsl"

out vec3 v_normal_ws;
out vec2 v_tex;
out vec3 v_pos_ws;
out vec3 v_pos_local;
flat out vec3 v_color;
flat out float v_alpha;
flat out int v_material_id;
flat out int v_color_role;
flat out int v_instance_id;
flat out int v_role_color_count;
flat out vec4 v_wear_params;

void main() {
  uint gid = uint(gl_VertexID);
  uint instance = gid / u_vertex_count;
  uint v = gid - instance * u_vertex_count;

  RiggedInstanceData data = rg_instance[instance];

  mat4 skin = rg_skin_matrix(v, instance);
  vec3 variation_scale = data.variation_material.xyz;
  vec4 pos_local = vec4(rg_position(v) * variation_scale, 1.0);
  vec4 skinned_local = skin * pos_local;
  vec4 pos_world = data.world * skinned_local;
  gl_Position = u_view_proj * pos_world;

  mat3 skin_rot = mat3(skin);
  mat3 model_rot = mat3(data.world);
  v_normal_ws = normalize(model_rot * skin_rot * rg_normal(v));
  v_pos_ws = pos_world.xyz;
  v_pos_local = skinned_local.xyz;
  v_tex = rg_texcoord(v);
  v_color = data.color_alpha.rgb;
  v_alpha = data.color_alpha.a;
  v_material_id = int(data.variation_material.w);
  v_color_role = int(rg_color_role(v));
  v_instance_id = int(instance);
  v_role_color_count = int(data.role_meta.x);
  v_wear_params = data.wear_params;
}
