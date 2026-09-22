#version 330 core
#include "directional_shadows.glsl"
#include "environment_lighting.glsl"
#include "local_lighting.glsl"
#include "material_detail.glsl"

in vec3 v_normal;
in vec2 v_tex_coord;
in vec3 v_world_pos;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_use_texture;
uniform float u_alpha;
uniform int u_material_id;

out vec4 frag_color;

float soi_resolve_ghost_alpha(float alpha) {
  if (alpha <= 1.0) {
    return alpha;
  }
  const float bayer[16] = float[16](0.0,
                                    8.0,
                                    2.0,
                                    10.0,
                                    12.0,
                                    4.0,
                                    14.0,
                                    6.0,
                                    3.0,
                                    11.0,
                                    1.0,
                                    9.0,
                                    15.0,
                                    7.0,
                                    13.0,
                                    5.0);
  ivec2 cell = ivec2(gl_FragCoord.xy) & ivec2(3);
  if ((bayer[cell.x + cell.y * 4] + 0.5) / 16.0 >= alpha - 1.0) {
    discard;
  }
  return 1.0;
}

void main() {
  vec3 color = u_color;
  if (u_use_texture) {
    color *= texture(u_texture, v_tex_coord).rgb;
  }

  vec3 normal = normalize(v_normal);
  int soi_material = u_material_id % 10;
  int soi_damage_tier = u_material_id / 10;
  color = soi_material_variation(color, v_world_pos, normal, soi_material);
  color = soi_apply_damage_soot(color, v_world_pos, soi_damage_tier);

  float avg_color = (color.r + color.g + color.b) / 3.0;
  float wrap_amount = avg_color > 0.65 ? 0.38 : (avg_color > 0.40 ? 0.16 : 0.04);
  vec3 albedo = color;
  color *= environment_lighting(normal, wrap_amount);
  color = apply_directional_shadow(color, v_world_pos, normal);
  color += albedo * local_lighting(v_world_pos, normal);
  frag_color = vec4(color, soi_resolve_ghost_alpha(u_alpha));
}
