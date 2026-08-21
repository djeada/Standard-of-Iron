#version 330 core
#include "directional_shadows.glsl"
#include "environment_lighting.glsl"
#include "local_lighting.glsl"
#include "material_detail.glsl"

in vec3 v_normal;
in vec2 v_tex_coord;
in vec3 v_world_pos;
flat in vec3 v_instance_color;
flat in float v_instance_alpha;

uniform sampler2D u_texture;
uniform bool u_use_texture;
uniform int u_material_id;

out vec4 frag_color;

void main() {
  vec3 color = v_instance_color;
  if (u_use_texture) {
    color *= texture(u_texture, v_tex_coord).rgb;
  }

  vec3 normal = normalize(v_normal);
  int soi_material = u_material_id % 10;
  int soi_damage_tier = u_material_id / 10;
  color = soi_material_variation(color, v_world_pos, normal, soi_material);
  color = soi_apply_damage_soot(color, v_world_pos, soi_damage_tier);

  float avg_color =
      (v_instance_color.r + v_instance_color.g + v_instance_color.b) / 3.0;
  float wrap_amount = avg_color > 0.65 ? 0.52 : (avg_color > 0.40 ? 0.20 : 0.05);
  vec3 albedo = color;
  color *= environment_lighting(normal, wrap_amount);
  color = apply_directional_shadow(color, v_world_pos, normal);
  color += albedo * local_lighting(v_world_pos, normal);
  frag_color = vec4(color, v_instance_alpha);
}
