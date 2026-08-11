#version 330 core
#include "directional_shadows.glsl"
#include "environment_lighting.glsl"
#include "local_lighting.glsl"
#include "noise.glsl"
#include "visibility_mask.glsl"

in vec3 v_normal;
in vec3 v_world_pos;
in vec3 v_color;
in vec2 v_tex_coord;
in float v_foliage_mask;
in float v_needle_seed;
in float v_bark_seed;
in vec3 v_local_pos;
in float v_bough;

out vec4 frag_color;

const float PI = 3.14159265359;
const float TWO_PI = 6.28318530718;

vec3 perturb_normal(vec3 n, vec3 world_pos, float height, float strength) {
  vec3 dpdx = dFdx(world_pos);
  vec3 dpdy = dFdy(world_pos);
  vec3 r1 = cross(dpdy, n);
  vec3 r2 = cross(n, dpdx);
  float det = dot(dpdx, r1);
  vec3 gradient = sign(det) * (dFdx(height) * r1 + dFdy(height) * r2);
  return normalize(abs(det) * n - strength * gradient);
}

void main() {

  vec3 geometric_normal = normalize(v_normal);
  vec3 l = environment_primary_direction();
  vec3 view_dir = normalize(vec3(0.0, 0.86, 0.52));

  vec3 needle_seed_offset =
      vec3(v_needle_seed * 41.0, v_needle_seed * 17.0, v_needle_seed * 63.0);

  float footprint = max(fwidth(v_local_pos.x), fwidth(v_local_pos.z));
  float fine_detail = 1.0 - smoothstep(0.006, 0.026, footprint);
  float mid_detail = 1.0 - smoothstep(0.016, 0.055, footprint);

  float needle_fine =
      soi_noise3(v_local_pos * vec3(30.0, 13.0, 30.0) + needle_seed_offset);
  float needle_clump = soi_noise3(v_local_pos * 11.0 + needle_seed_offset.zxy);
  float needle_tuft = soi_noise3(v_local_pos * 4.6 + needle_seed_offset.yzx);
  needle_fine = mix(0.5, needle_fine, fine_detail);
  needle_clump = mix(0.5, needle_clump, mid_detail);

  vec3 needle_deep = vec3(0.038, 0.096, 0.058);
  vec3 needle_mid = vec3(0.112, 0.226, 0.122);
  vec3 needle_light = vec3(0.252, 0.398, 0.190);
  vec3 needle_sun = vec3(0.345, 0.505, 0.252);

  float grain =
      clamp(needle_clump * 0.80 + (needle_fine - 0.5) * 0.20 + 0.10, 0.0, 1.0);
  vec3 needle_color = mix(needle_deep, needle_mid, grain);
  needle_color =
      mix(needle_color, needle_light, smoothstep(0.48, 0.92, needle_tuft) * 0.45);

  needle_color = mix(needle_color, v_color, 0.42);

  float bump_height = needle_clump + needle_tuft * 0.55;
  vec3 n = mix(geometric_normal,
               perturb_normal(geometric_normal, v_world_pos, bump_height, 0.20),
               v_foliage_mask * mid_detail);

  vec3 shading_normal =
      normalize(mix(n, n + vec3(0.0, 1.2, 0.0), v_foliage_mask * 0.50));

  float ndl = dot(shading_normal, l);
  float diffuse = max(ndl, 0.0);
  float wrap = clamp((ndl + 0.26) / 1.26, 0.0, 1.0);
  wrap *= wrap * (3.0 - 2.0 * wrap);
  float backlight = max(-dot(n, l), 0.0);

  vec3 sun = environment_primary_color() * environment_primary_intensity();
  vec3 sky = environment_sky_color();
  vec3 illumination =
      environment_ambient_light(geometric_normal) * mix(1.0, 1.30, v_foliage_mask) +
      sun * mix(diffuse * 0.72, wrap, v_foliage_mask);

  float sun_catch = smoothstep(0.45, 1.00, wrap) * mix(0.20, 0.62, needle_clump);
  needle_color = mix(needle_color, needle_sun, sun_catch * v_foliage_mask);

  float underside = 1.0 - smoothstep(-0.30, 0.10, geometric_normal.y);
  float radius = length(v_local_pos.xz);
  float canopy_core = 1.0 - smoothstep(0.06, 0.42, radius);
  float shelf_ao = underside * (0.40 + canopy_core * 0.55);
  float bough_edge = smoothstep(0.30, 0.95, v_bough) * (1.0 - underside);
  float canopy_height = clamp(v_tex_coord.y, 0.0, 1.12);

  float shelf = mix(1.0, 0.46, clamp(shelf_ao, 0.0, 1.0));
  float core = mix(1.0, 0.74, canopy_core * 0.85);
  float tier = mix(0.84, 1.12, smoothstep(0.34, 1.06, canopy_height));
  float bough_lift = mix(1.0, 1.12, bough_edge);
  float hemi = clamp(geometric_normal.y * 0.5 + 0.5, 0.0, 1.0);
  float canopy_occlusion = clamp(shelf * core * tier * bough_lift, 0.36, 1.12);
  float ao = mix(1.0, canopy_occlusion, v_foliage_mask) * mix(0.72, 1.0, hemi);

  float old_needles = (1.0 - smoothstep(0.42, 0.70, v_tex_coord.y)) *
                      smoothstep(0.72, 0.96, needle_fine);
  needle_color = mix(needle_color, vec3(0.34, 0.265, 0.145), old_needles * 0.30);

  float bark_stripe = sin(v_tex_coord.y * 45.0 + v_bark_seed * TWO_PI) * 0.1 + 0.9;
  float bark_noise = soi_hash_15a407(vec2(v_tex_coord.x * 18.0 + v_bark_seed * 4.3,
                                          v_tex_coord.y * 10.0 + v_bark_seed * 7.7));

  vec3 bark_grey = vec3(0.36, 0.32, 0.28);
  vec3 bark_red = vec3(0.48, 0.32, 0.21);
  vec3 trunk_base = mix(bark_grey, bark_red, v_bark_seed) * bark_stripe;
  vec3 trunk_color = trunk_base * (0.82 + bark_noise * 0.40);
  float trunk_moss = (1.0 - smoothstep(0.02, 0.24, v_local_pos.y)) *
                     smoothstep(0.55, 0.86, bark_noise);
  trunk_color = mix(trunk_color, vec3(0.20, 0.25, 0.17), trunk_moss * 0.48);

  trunk_color *= mix(0.68, 1.0, smoothstep(0.0, 0.16, v_local_pos.y));
  trunk_color *= mix(1.0, 0.62, smoothstep(0.30, 0.58, v_local_pos.y));

  vec3 base_color = mix(trunk_color, needle_color, v_foliage_mask);
  vec3 color = base_color * illumination * ao * environment_exposure();

  float translucency = backlight * backlight *
                       (0.20 + smoothstep(0.82, 1.02, v_tex_coord.y) * 0.22) *
                       v_foliage_mask * (0.35 + bough_edge * 0.85);
  color += needle_color * vec3(0.34, 0.46, 0.20) * translucency * sun;

  vec3 half_dir = normalize(l + view_dir);
  float needle_spec =
      pow(max(dot(shading_normal, half_dir), 0.0), 20.0) * v_foliage_mask * 0.045;
  float rim = pow(1.0 - max(dot(geometric_normal, view_dir), 0.0), 4.0) *
              mix(0.055, 0.085, v_foliage_mask);
  color += sun * needle_spec;
  color += sky * rim;

  if (v_tex_coord.y < 0.028)
    discard;

  color += color * local_lighting(v_world_pos, n);
  color = apply_directional_shadow(color, v_world_pos, geometric_normal);
  color = apply_visibility_memory(color, v_world_pos.xz);
  frag_color = vec4(color, 1.0);
}
