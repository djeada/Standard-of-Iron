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
in float v_leaf_seed;
in float v_bark_seed;
in float v_branch_id;
in vec2 v_local_pos_xz;
in vec3 v_local_pos;

out vec4 frag_color;

const float PI = 3.14159265359;
const float TWO_PI = 6.28318530718;

float noise2_d(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float a = soi_hash_15a407(i);
  float b = soi_hash_15a407(i + vec2(1.0, 0.0));
  float c = soi_hash_15a407(i + vec2(0.0, 1.0));
  float d = soi_hash_15a407(i + vec2(1.0, 1.0));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main() {

  vec3 geometric_normal = normalize(v_normal);
  vec3 l = environment_primary_direction();

  vec3 leaf_seed_offset =
      vec3(v_leaf_seed * 37.0, v_branch_id * 11.0, v_leaf_seed * 59.0);
  float footprint = max(fwidth(v_local_pos.x), fwidth(v_local_pos.z));
  float fine_detail = 1.0 - smoothstep(0.004, 0.020, footprint);
  float mid_detail = 1.0 - smoothstep(0.012, 0.048, footprint);

  float leaf_fine = soi_noise3(v_local_pos * vec3(26.0, 21.0, 26.0) + leaf_seed_offset);
  float leaf_clump = soi_noise3(v_local_pos * 9.5 + leaf_seed_offset.zxy);
  float leaf_mass = soi_noise3(v_local_pos * 4.5 + leaf_seed_offset.yzx);
  leaf_fine = mix(0.5, leaf_fine, fine_detail);
  leaf_clump = mix(0.5, leaf_clump, mid_detail);

  float canopy_height = clamp((v_tex_coord.y - 0.52) / 0.58, 0.0, 1.0);
  float canopy_radius = length(v_local_pos_xz);
  float canopy_edge = smoothstep(0.10, 0.32, canopy_radius);
  float canopy_core = 1.0 - smoothstep(0.14, 0.40, canopy_radius);

  vec3 leaf_dark_green = vec3(0.085, 0.150, 0.100);
  vec3 leaf_mid_green = vec3(0.215, 0.310, 0.175);
  vec3 leaf_light_green = vec3(0.375, 0.455, 0.250);
  vec3 leaf_silver = vec3(0.500, 0.540, 0.455);

  float color_choice =
      clamp(leaf_clump * 0.90 + (leaf_fine - 0.5) * 0.26 + 0.10, 0.0, 1.0);
  vec3 leaf_color = mix(leaf_dark_green, leaf_mid_green, color_choice);
  leaf_color =
      mix(leaf_color, leaf_light_green, smoothstep(0.40, 0.90, leaf_mass) * 0.62);

  vec3 n = geometric_normal;

  float ndl = dot(n, l);
  float diffuse = max(ndl, 0.0);
  float wrap = clamp((ndl + 0.24) / 1.24, 0.0, 1.0);
  wrap *= wrap * (3.0 - 2.0 * wrap);
  float backlight = max(-ndl, 0.0);
  float ambient = environment_ambient_intensity();
  float sky_fill = smoothstep(-0.20, 0.85, n.y) * mix(0.05, 0.14, v_foliage_mask);
  float lighting = ambient + wrap * mix(0.30, 0.66, v_foliage_mask) + sky_fill;

  vec3 sun_color = environment_primary_color() * environment_primary_intensity();
  vec3 sky_color = environment_sky_color();
  float lit_t = clamp(wrap * 1.2, 0.0, 1.0);
  vec3 light_tint = mix(sky_color * 0.55, sun_color, lit_t);

  float silver_show =
      smoothstep(0.30, 0.80, 1.0 - diffuse) * smoothstep(0.35, 0.85, leaf_clump);
  leaf_color =
      mix(leaf_color, leaf_silver, silver_show * mix(0.12, 0.24, canopy_height));

  leaf_color = mix(leaf_color, v_color, 0.34);

  leaf_color *= mix(vec3(0.84, 0.95, 1.10), vec3(1.08, 1.03, 0.85), lit_t);

  float underside = 1.0 - smoothstep(-0.10, 0.42, geometric_normal.y);
  float canopy_ao = mix(0.74, 1.06, canopy_edge) * mix(0.90, 1.08, canopy_height);
  leaf_color *= canopy_ao;
  leaf_color *= mix(1.0, 0.66, canopy_core * v_foliage_mask);
  leaf_color *= mix(1.0, 0.78, underside * v_foliage_mask);

  float bark_u = v_tex_coord.x * TWO_PI;
  float bark_v = v_tex_coord.y;

  float furrows = pow(abs(sin(bark_u * 5.0 + v_bark_seed * TWO_PI)), 0.4);
  float vertical_grain =
      noise2_d(vec2(bark_u * 3.0, bark_v * 25.0 + v_bark_seed * 7.0));
  float bark_noise = noise2_d(vec2(bark_u * 8.0, bark_v * 15.0)) * 0.3;
  float bark_knots = noise2_d(vec2(bark_u * 2.6 + v_leaf_seed * 4.0, bark_v * 9.0));
  float bark_texture =
      furrows * 0.46 + vertical_grain * 0.32 + bark_noise + bark_knots * 0.18;

  vec3 bark_dark = vec3(0.25, 0.23, 0.20);
  vec3 bark_mid = vec3(0.40, 0.37, 0.31);
  vec3 bark_light = vec3(0.52, 0.49, 0.42);
  vec3 bark_lichen = vec3(0.30, 0.33, 0.27);

  vec3 bark_color = mix(bark_dark, bark_mid, bark_texture);
  float bark_highlight =
      smoothstep(0.72, 0.95, soi_hash_15a407(vec2(bark_v * 15.0, bark_u * 3.0)));
  bark_color = mix(bark_color, bark_light, bark_highlight * 0.38);
  bark_color = mix(bark_color, bark_lichen, smoothstep(0.72, 0.96, bark_knots) * 0.14);
  float basal_lichen = (1.0 - smoothstep(0.04, 0.30, v_local_pos.y)) *
                       smoothstep(0.50, 0.84, vertical_grain);
  bark_color = mix(bark_color, bark_lichen, basal_lichen * 0.42);
  bark_color *= mix(0.72, 1.0, smoothstep(0.0, 0.14, v_local_pos.y));

  vec3 base_color = mix(bark_color, leaf_color, v_foliage_mask);
  vec3 color = base_color * lighting * light_tint * environment_exposure();

  float translucency =
      backlight * backlight * v_foliage_mask * (0.14 + canopy_edge * 0.26);
  color += leaf_color * vec3(0.42, 0.50, 0.22) * translucency;

  float leaf_sheen = pow(max(dot(n, normalize(l + vec3(0.0, 0.9, 0.3))), 0.0), 12.0) *
                     v_foliage_mask * 0.085;
  color += mix(sky_color, vec3(1.0), 0.35) * leaf_sheen;

  if (v_tex_coord.y < 0.035)
    discard;

  color += color * local_lighting(v_world_pos, n);
  color = apply_directional_shadow(color, v_world_pos, geometric_normal);
  color = apply_visibility_memory(color, v_world_pos.xz);
  frag_color = vec4(color, 1.0);
}
