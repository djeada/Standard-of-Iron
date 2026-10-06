#version 330 core
#include "directional_shadows.glsl"
#include "environment_lighting.glsl"
#include "local_lighting.glsl"
#include "noise.glsl"
#include "visibility_mask.glsl"

in vec3 v_normal;
in vec3 v_world_pos;
in vec3 v_color;
in vec3 v_tex_coord;
in vec3 v_local_pos;
in float v_frond_reach;
in float v_leaf_seed;
in float v_bark_seed;

uniform vec3 u_camera_pos;

out vec4 frag_color;

const float TWO_PI = 6.28318530718;

void main() {
  vec3 geometric_normal = normalize(v_normal);
  if (!gl_FrontFacing) {
    geometric_normal = -geometric_normal;
  }
  vec3 l = environment_primary_direction();
  vec3 view_dir = normalize(u_camera_pos - v_world_pos);

  float frond = step(0.75, v_tex_coord.z);
  float live = step(1.75, v_tex_coord.z);
  float fibre = clamp(v_tex_coord.z * 2.0, 0.0, 1.0);
  float across = clamp((v_tex_coord.z - 1.0 - live) * 2.0, 0.0, 1.0) * frond;
  float along = v_frond_reach;

  float footprint = max(fwidth(v_local_pos.x), fwidth(v_local_pos.z));
  float fine_detail = 1.0 - smoothstep(0.006, 0.026, footprint);
  float mid_detail = 1.0 - smoothstep(0.016, 0.055, footprint);

  float frond_id = v_tex_coord.x + v_leaf_seed * 0.37;
  float frond_hue = soi_hash_15a407(vec2(frond_id * 31.0, v_leaf_seed * 7.0));
  float tree_hue = fract(v_leaf_seed * 3.71 + v_bark_seed * 1.37);

  float leaflet_phase = along * 34.0 - across * 3.2 + frond_id * 11.0;
  float leaflet = abs(fract(leaflet_phase) - 0.5) * 2.0;
  float leaflet_gap = smoothstep(0.62, 0.95, leaflet) * smoothstep(0.30, 0.75, across);
  leaflet_gap *= mid_detail;
  float midrib = (1.0 - smoothstep(0.0, 0.16, across)) * fine_detail;

  vec3 frond_dark = vec3(0.080, 0.130, 0.058);
  vec3 frond_mid = vec3(0.180, 0.262, 0.105);
  vec3 frond_light = vec3(0.320, 0.400, 0.170);
  vec3 frond_sun = vec3(0.520, 0.580, 0.250);
  vec3 frond_rib = vec3(0.560, 0.540, 0.300);

  vec3 leaf_color = mix(frond_mid, frond_light, frond_hue * 0.45 + along * 0.25);
  leaf_color = mix(leaf_color, v_color * 1.25, 0.30);
  leaf_color *= mix(vec3(0.84, 0.90, 1.0), vec3(1.06, 1.0, 0.86), tree_hue);
  leaf_color = mix(leaf_color, frond_dark, leaflet_gap * 0.55);
  leaf_color = mix(leaf_color, frond_dark, (1.0 - along) * 0.35);
  leaf_color = mix(leaf_color, frond_rib, midrib * 0.45);
  float tip_burn =
      smoothstep(0.82, 1.0, along) * smoothstep(0.55, 0.90, frond_hue) * 0.45;
  leaf_color = mix(leaf_color, vec3(0.46, 0.40, 0.20), tip_burn);

  float straw_grain = soi_hash_15a407(
      vec2(across * 9.0 + frond_id * 13.0, along * 40.0 + v_local_pos.y * 30.0));
  vec3 straw_dark = vec3(0.230, 0.170, 0.105);
  vec3 straw_light = vec3(0.470, 0.365, 0.215);
  vec3 dead_color = mix(straw_dark, straw_light, straw_grain * 0.6 + along * 0.4);

  float bark_u = v_tex_coord.x * TWO_PI;
  float scar_row = v_local_pos.y * 30.0 + v_bark_seed * 3.0;
  float scar_a = abs(fract(scar_row + bark_u * 0.95) - 0.5) * 2.0;
  float scar_b = abs(fract(scar_row - bark_u * 0.95) - 0.5) * 2.0;
  float scars = smoothstep(0.55, 0.92, max(scar_a, scar_b)) * mid_detail;
  float bark_noise =
      soi_hash_15a407(vec2(bark_u * 4.0 + v_bark_seed * 9.0, v_local_pos.y * 22.0));
  vec3 bark_dark = vec3(0.200, 0.160, 0.120);
  vec3 bark_mid = vec3(0.400, 0.340, 0.260);
  vec3 bark_color = mix(bark_mid, bark_dark, scars * 0.70);
  bark_color *= 0.86 + bark_noise * 0.24;
  bark_color = mix(bark_color, vec3(0.40, 0.33, 0.24), v_bark_seed * 0.35);
  bark_color *= mix(0.78, 1.0, smoothstep(0.0, 0.10, v_local_pos.y));

  vec3 fibre_color = mix(straw_dark * 1.2, straw_light, straw_grain * 0.7);
  vec3 woody = mix(bark_color, fibre_color, fibre);
  vec3 frond_color = mix(dead_color, leaf_color, live);
  vec3 base_color = mix(woody, frond_color, frond);

  float ndl = dot(geometric_normal, l);
  float wrap = clamp((ndl + 0.30) / 1.30, 0.0, 1.0);
  wrap *= wrap * (3.0 - 2.0 * wrap);
  float backlight = max(-ndl, 0.0);

  vec3 sun = environment_primary_color() * environment_primary_intensity();
  vec3 sky = environment_sky_color();
  vec3 illumination = environment_ambient_light(geometric_normal) +
                      soi_key_light(geometric_normal) +
                      soi_canopy_scatter(geometric_normal) * frond * 0.6;

  float sun_catch = smoothstep(0.45, 1.0, wrap) * live * (1.0 - leaflet_gap * 0.6);
  base_color = mix(base_color, frond_sun, sun_catch * 0.35);

  float crown_shade = (1.0 - along) * (1.0 - along) * frond * 0.50;
  float hemi = clamp(geometric_normal.y * 0.5 + 0.5, 0.0, 1.0);
  float ao = mix(0.78, 1.0, hemi) * (1.0 - crown_shade);
  ao *= mix(1.0, 0.82, (1.0 - frond) * fibre);

  vec3 color = base_color * illumination * ao * environment_exposure();

  float translucency =
      backlight * live * (0.30 + along * 0.45) * (1.0 - leaflet_gap * 0.5);
  color += leaf_color * vec3(0.62, 0.72, 0.30) * translucency * sun;

  vec3 half_dir = normalize(l + view_dir);
  float waxy = pow(max(dot(geometric_normal, half_dir), 0.0), 24.0) * live * 0.10;
  float rim = pow(1.0 - max(dot(geometric_normal, view_dir), 0.0), 4.0) *
              mix(0.05, 0.10, frond);
  color += sun * waxy;
  color += sky * rim;

  color = apply_directional_shadow(color, v_world_pos, geometric_normal);
  color += base_color * ao * local_lighting(v_world_pos, geometric_normal);
  color = apply_visibility_world_shading(color, v_world_pos.xz);
  frag_color = vec4(color, 1.0);
}
