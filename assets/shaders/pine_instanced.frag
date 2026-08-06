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

  vec3 needle_seed_offset =
      vec3(v_needle_seed * 41.0, v_needle_seed * 17.0, v_needle_seed * 63.0);

  float footprint = max(fwidth(v_local_pos.x), fwidth(v_local_pos.z));
  float fine_detail = 1.0 - smoothstep(0.010, 0.045, footprint);
  float mid_detail = 1.0 - smoothstep(0.026, 0.090, footprint);

  float needle_fine =
      soi_noise3(v_local_pos * vec3(30.0, 13.0, 30.0) + needle_seed_offset);
  float needle_clump = soi_noise3(v_local_pos * 11.0 + needle_seed_offset.zxy);
  float needle_tuft = soi_noise3(v_local_pos * 4.6 + needle_seed_offset.yzx);
  needle_fine = mix(0.5, needle_fine, fine_detail);
  needle_clump = mix(0.5, needle_clump, mid_detail);

  vec3 needle_deep = vec3(0.042, 0.105, 0.070);
  vec3 needle_mid = vec3(0.115, 0.250, 0.125);
  vec3 needle_light = vec3(0.235, 0.420, 0.175);
  vec3 needle_sun = vec3(0.470, 0.605, 0.250);

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

  float ndl = dot(n, l);
  float diffuse = max(ndl, 0.0);
  float wrap = clamp((ndl + 0.26) / 1.26, 0.0, 1.0);
  wrap *= wrap * (3.0 - 2.0 * wrap);
  float backlight = max(-ndl, 0.0);
  float ambient = environment_ambient_intensity();
  float sky_fill = smoothstep(-0.25, 0.85, n.y) * mix(0.08, 0.16, v_foliage_mask);
  float lighting = ambient + wrap * mix(0.42, 0.70, v_foliage_mask) + sky_fill;

  vec3 sun_color = environment_primary_color() * environment_primary_intensity();
  vec3 sky_color = environment_sky_color();
  float lit_t = clamp(wrap * 1.15, 0.0, 1.0);
  vec3 light_tint = mix(sky_color * 0.55, sun_color, lit_t);

  float underside = 1.0 - smoothstep(0.02, 0.44, geometric_normal.y);
  float radius = length(v_local_pos.xz);
  float canopy_core = 1.0 - smoothstep(0.06, 0.42, radius);
  float shelf_ao = underside * (0.40 + canopy_core * 0.55);
  needle_color *= mix(1.0, 0.38, clamp(shelf_ao, 0.0, 1.0) * v_foliage_mask);

  float bough_edge = smoothstep(0.30, 0.95, v_bough) * (1.0 - underside);
  needle_color *= mix(1.0, 1.14, bough_edge * v_foliage_mask);
  needle_color *= mix(1.0, 0.72, canopy_core * v_foliage_mask * 0.85);

  float canopy_height = clamp(v_tex_coord.y, 0.0, 1.12);
  needle_color *= mix(0.80, 1.14, smoothstep(0.34, 1.06, canopy_height));

  float tip_lit = smoothstep(0.20, 0.80, diffuse) * bough_edge;
  needle_color = mix(needle_color, needle_sun, tip_lit * 0.26 * needle_clump);

  needle_color *= mix(vec3(0.86, 0.96, 1.10), vec3(1.07, 1.02, 0.86), lit_t);

  float old_needles = (1.0 - smoothstep(0.42, 0.70, v_tex_coord.y)) *
                      smoothstep(0.72, 0.96, needle_fine);
  needle_color = mix(needle_color, vec3(0.30, 0.235, 0.125), old_needles * 0.30);

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
  vec3 color = base_color * lighting * light_tint * environment_exposure();

  float translucency = backlight * backlight *
                       (0.20 + smoothstep(0.82, 1.02, v_tex_coord.y) * 0.22) *
                       v_foliage_mask * (0.35 + bough_edge * 0.85);
  color += needle_color * vec3(0.34, 0.46, 0.20) * translucency;

  float needle_sheen =
      pow(max(dot(n, normalize(l + vec3(0.0, 0.8, 0.35))), 0.0), 14.0) *
      v_foliage_mask * 0.055;
  color += mix(sky_color, sun_color, 0.5) * needle_sheen;

  if (v_tex_coord.y < 0.028)
    discard;

  color += color * local_lighting(v_world_pos, n);
  color = apply_directional_shadow(color, v_world_pos, geometric_normal);
  color = apply_visibility_memory(color, v_world_pos.xz);
  frag_color = vec4(color, 1.0);
}
