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
flat in int v_material_id;
flat in float v_ground_height;
flat in vec4 v_instance_tint;

uniform sampler2D u_texture;
uniform bool u_use_texture;

out vec4 frag_color;

const float k_plinth_height = 0.90;
const float k_plinth_strength = 0.65;
const vec3 k_plinth_tint = vec3(0.66, 0.60, 0.52);
const vec3 k_lamplight_spill = vec3(0.30, 0.17, 0.07);
const float k_street_bounce_gain = 2.4;
// The darkest roof tone soi_building_instance_tint() hands out, and the hue an
// old roof drifts to.
const float k_roof_tone_low = 0.74;
const vec3 k_roof_tone_old = vec3(0.97, 0.90, 0.86);
// How much of its orange the most faded roof keeps, and what it fades to.
const float k_roof_colour_low = 0.45;
const vec3 k_roof_faded = vec3(1.10, 1.00, 0.88);

float soi_resolve_ghost_alpha(float alpha) {
#ifdef SOI_OPAQUE_ONLY
  return alpha;
#else
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
#endif
}

void main() {
  vec3 color = v_instance_color;
  if (u_use_texture) {
    color *= texture(u_texture, v_tex_coord).rgb;
  }

  vec3 normal = normalize(v_normal);
  int soi_material = v_material_id % 10;
  int soi_damage_tier = v_material_id / 10;
  vec3 surface_normal = normal;
  normal = soi_material_normal(v_world_pos, normal, soi_material);
  float tint_luma = dot(color, vec3(0.299, 0.587, 0.114));
  float tint_chroma =
      max(color.r, max(color.g, color.b)) - min(color.r, min(color.g, color.b));
  float plaster =
      smoothstep(0.55, 0.70, tint_luma) * (1.0 - smoothstep(0.10, 0.22, tint_chroma));
  float terracotta = smoothstep(0.18, 0.30, color.r - color.b) *
                     (1.0 - smoothstep(0.62, 0.80, tint_luma));
  if (soi_material == k_material_mineral || soi_material == k_material_ceramic) {
    color *= mix(vec3(1.0), v_instance_tint.rgb, plaster);
    // Every roof has weathered to its own tone, from sun-bleached to mossy
    // brown, so a town seen from above is not one sheet of orange.
    // A tint of exactly one (anything that is not a placed building) is left
    // untouched.
    float roof_fresh = smoothstep(k_roof_tone_low, 1.0, v_instance_tint.a);
    vec3 roof_tone = mix(k_roof_tone_old, vec3(1.0), roof_fresh) * v_instance_tint.a;
    // And its own depth of colour: new tile is orange, old tile has gone to
    // buff and brown. Drawn from the same tone, so no two roofs agree on both.
    float roof_age =
        abs(v_instance_tint.a - 1.0) > 1e-4 ? fract(v_instance_tint.a * 53.7) : 1.0;
    float roof_luma = dot(color, vec3(0.299, 0.587, 0.114));
    vec3 roof_color = mix(
        vec3(roof_luma) * k_roof_faded, color, mix(k_roof_colour_low, 1.0, roof_age));
    color = mix(color, roof_color * roof_tone, terracotta);
  }
  color = soi_material_variation(color, v_world_pos, normal, soi_material, v_tex_coord);
  color = soi_apply_damage_soot(color, v_world_pos, soi_damage_tier);

  float wall_face = 1.0 - smoothstep(0.55, 0.90, abs(surface_normal.y));
  float damp_line =
      u_has_material_detail ? soi_detail_coarse(v_world_pos.xz * 1.7) * 0.28 : 0.14;
  float plinth =
      1.0 -
      smoothstep(0.0, k_plinth_height, v_world_pos.y - v_ground_height - damp_line);
  if (soi_material == k_material_mineral || soi_material == k_material_wood ||
      soi_material == k_material_ceramic) {
    color = mix(color, color * k_plinth_tint, plinth * wall_face * k_plinth_strength);
  }

  float avg_color = (color.r + color.g + color.b) / 3.0;
  float wrap_amount = avg_color > 0.65 ? 0.38 : (avg_color > 0.40 ? 0.16 : 0.04);
  if (soi_material == k_material_plant) {
    wrap_amount = 0.28;
  }
  vec3 albedo = color;
  color *= environment_lighting(normal, wrap_amount);
  color += soi_material_response(albedo, normal, v_world_pos, soi_material);
  float shadow_amount = directional_shadow_amount(v_world_pos, surface_normal);
  color = mix(color, color * environment_shadow_tint(), shadow_amount);
  // A wall the sun does not reach is still lit by the sunlit street and the
  // houses across it. Without that fill, shaded plaster takes only the sky and
  // the cool shadow tint and turns slate blue.
  float unlit = max(shadow_amount, 1.0 - soi_wrapped_diffuse(normal));
  color += albedo * environment_sun_bounce() *
           (unlit * wall_face * k_street_bounce_gain * environment_exposure());
  color += albedo * local_lighting(v_world_pos, normal);

  float night = environment_night_amount();
  if (night > 0.0 && soi_material == k_material_wood && v_instance_tint.a < 1.03) {
    float storey = v_world_pos.y - v_ground_height;
    float opening = (1.0 - smoothstep(0.24, 0.31, tint_luma)) * wall_face *
                    smoothstep(0.35, 0.65, storey) *
                    (1.0 - smoothstep(2.6, 3.4, storey));
    color += k_lamplight_spill * opening * night;
  }
  frag_color = vec4(color, soi_resolve_ghost_alpha(v_instance_alpha));
}
