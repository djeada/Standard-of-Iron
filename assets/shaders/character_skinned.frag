#version 330 core
#include "character_shading.glsl"
#include "directional_shadows.glsl"
#include "environment_lighting.glsl"
#include "local_lighting.glsl"
#include "noise.glsl"

in vec3 v_normal_ws;
in vec2 v_tex;
in vec3 v_pos_ws;
in vec3 v_pos_local;
flat in int v_color_role;
flat in vec4 v_wear_params;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_use_texture;
uniform float u_alpha;
uniform int u_material_id;
uniform vec3 u_role_colors[32];
uniform int u_role_color_count;

out vec4 frag_color;

void main() {
  vec3 base = u_color;
  if (v_color_role > 0 && v_color_role <= u_role_color_count) {
    base = u_role_colors[v_color_role - 1];
  }
  if (u_use_texture) {
    base *= texture(u_texture, v_tex).rgb;
  }
  float zoom = readable_zoom(v_pos_ws);

  vec4 readable_wear = v_wear_params;
  readable_wear.x *= mix(1.0, k_readable_wear_far, zoom);
  readable_wear.y *= mix(1.0, k_readable_grime_far, zoom);
  readable_wear.z *= mix(1.0, k_readable_blood_far, zoom);
#if SOI_SURFACE_DETAIL
  base = apply_wear(base, u_material_id, v_color_role, v_pos_local, readable_wear);
#endif

  vec3 surface_normal = normalize(v_normal_ws);
  vec3 color = shade_readable_character(
      base, surface_normal, v_pos_ws, u_material_id, v_color_role, zoom);
  color = apply_directional_shadow(color, v_pos_ws, surface_normal);

  vec3 local_light = local_lighting(v_pos_ws, surface_normal);
  local_light = local_light / (vec3(1.0) + local_light * 0.55);
  color += base * local_light;
  if (u_material_id == 0) {
    color *= mix(0.80, 1.0, smoothstep(0.0, 0.32, v_pos_local.y));
  }
  color = soi_finish_character(color,
                               base,
                               surface_normal,
                               v_pos_ws,
                               u_camera_position,
                               u_material_id,
                               v_color_role,
                               zoom);
  frag_color = vec4(color, u_alpha);
}
