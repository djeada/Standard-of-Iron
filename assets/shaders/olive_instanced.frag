#version 330 core
#include "directional_shadows.glsl"
#include "environment_lighting.glsl"
#include "foliage_bump.glsl"
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
flat in float v_branch_id;
in vec2 v_local_pos_xz;
in vec3 v_local_pos;

uniform vec3 u_camera_pos;

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
  vec3 view_dir = normalize(u_camera_pos - v_world_pos);

  vec3 leaf_seed_offset =
      vec3(v_leaf_seed * 37.0, v_branch_id * 11.0, v_leaf_seed * 59.0);
  float footprint = max(fwidth(v_local_pos.x), fwidth(v_local_pos.z));
  float fine_detail = 1.0 - smoothstep(0.006, 0.026, footprint);
  float mid_detail = 1.0 - smoothstep(0.016, 0.055, footprint);

  float leaf_fine_raw =
      soi_noise3(v_local_pos * vec3(26.0, 21.0, 26.0) + leaf_seed_offset);
  float leaf_clump_raw = soi_noise3(v_local_pos * 9.5 + leaf_seed_offset.zxy);
  float leaf_mass = soi_noise3(v_local_pos * 4.5 + leaf_seed_offset.yzx);
  float leaf_sprig = soi_noise3(v_local_pos * 15.0 + leaf_seed_offset.xzy * 1.7);
  float leaf_fine = mix(0.5, leaf_fine_raw, fine_detail);
  float leaf_clump = mix(0.5, leaf_clump_raw, mid_detail);
  leaf_sprig = mix(0.5, leaf_sprig, mid_detail);

  float canopy_height = clamp((v_tex_coord.y - 0.52) / 0.58, 0.0, 1.0);
  float canopy_radius = length(v_local_pos_xz);
  float canopy_edge = smoothstep(0.10, 0.32, canopy_radius);
  float canopy_core = 1.0 - smoothstep(0.14, 0.40, canopy_radius);

  float silhouette = 1.0 - abs(dot(geometric_normal, view_dir));

  vec3 leaf_dark_green = vec3(0.078, 0.110, 0.084);
  vec3 leaf_mid_green = vec3(0.196, 0.246, 0.172);
  vec3 leaf_light_green = vec3(0.340, 0.392, 0.270);
  vec3 leaf_silver = vec3(0.600, 0.640, 0.590);
  vec3 leaf_sun = vec3(0.470, 0.530, 0.362);

  float color_choice =
      clamp(leaf_clump * 0.90 + (leaf_fine - 0.5) * 0.26 + 0.10, 0.0, 1.0);
  vec3 leaf_color = mix(leaf_dark_green, leaf_mid_green, color_choice);
  leaf_color =
      mix(leaf_color, leaf_light_green, smoothstep(0.40, 0.90, leaf_mass) * 0.62);

  leaf_color = mix(leaf_color, v_color, 0.24);

  float lobe_height = 0.0;
  float sprig_height = 0.0;
  vec3 lobed = lobe_normal(
      geometric_normal, v_local_pos, 10.0, 0.42, leaf_seed_offset, lobe_height);
  lobed = lobe_normal(
      lobed, v_local_pos, 24.0, 0.22 * mid_detail, leaf_seed_offset.zyx, sprig_height);
  vec3 n = normalize(mix(geometric_normal, lobed, v_foliage_mask));
  float crevice =
      smoothstep(-0.55, 0.35, lobe_height + sprig_height * 0.5 * mid_detail);
  vec3 shading_normal =
      normalize(mix(n, n + vec3(0.0, 1.2, 0.0), v_foliage_mask * 0.50));

  float underside = 1.0 - smoothstep(-0.35, 0.05, geometric_normal.y);
  float ndl = dot(shading_normal, l);
  float diffuse = max(ndl, 0.0);
  float wrap = clamp((ndl + 0.24) / 1.24, 0.0, 1.0);
  wrap *= wrap * (3.0 - 2.0 * wrap);
  float backlight = max(-dot(n, l), 0.0);

  vec3 sun = environment_primary_color() * environment_primary_intensity();
  vec3 sky = environment_sky_color();
  vec3 illumination =
      environment_ambient_light(geometric_normal) * mix(1.0, 1.06, v_foliage_mask) +
      soi_key_light(geometric_normal) * mix(0.72, 1.0, v_foliage_mask);

  float silver_show =
      smoothstep(0.22, 0.75, 1.0 - diffuse) * smoothstep(0.30, 0.80, leaf_clump);
  silver_show = max(silver_show, smoothstep(0.58, 0.95, leaf_fine) * 0.70);
  silver_show = max(silver_show, underside * smoothstep(0.35, 0.75, leaf_clump) * 0.80);
  silver_show += canopy_edge * smoothstep(0.45, 0.85, leaf_fine) * 0.35;
  float turned_leaves =
      smoothstep(0.56, 0.86, leaf_sprig) * (0.40 + 0.60 * silhouette) * mid_detail;
  silver_show = max(silver_show, turned_leaves);
  leaf_color = mix(leaf_color,
                   leaf_silver,
                   clamp(silver_show, 0.0, 1.0) * mix(0.26, 0.48, canopy_height));

  float sun_catch = smoothstep(0.43, 1.00, wrap) * mix(0.20, 0.62, leaf_clump);
  leaf_color = mix(leaf_color, leaf_sun, sun_catch * v_foliage_mask);

  float hemi = clamp(geometric_normal.y * 0.5 + 0.5, 0.0, 1.0);
  float clump_shadow = 1.0 - smoothstep(0.30, 0.70, leaf_mass) * 0.30;
  float canopy_shape = mix(0.82, 1.08, canopy_edge) * mix(0.90, 1.10, canopy_height);
  float canopy_occlusion = clamp(canopy_shape * mix(1.0, 0.72, canopy_core) *
                                     mix(1.0, 0.84, underside) * clump_shadow,
                                 0.38,
                                 1.14);
  float ao = mix(1.0, canopy_occlusion, v_foliage_mask) * mix(0.72, 1.0, hemi);
  ao *= mix(1.0, mix(0.70, 1.04, crevice), v_foliage_mask);

  float bark_u = v_tex_coord.x * TWO_PI;
  float bark_v = v_tex_coord.y;

  float furrow_wave = bark_u * 5.0 + bark_v * 6.0 + v_bark_seed * TWO_PI;
  float furrows = pow(abs(sin(furrow_wave)), 0.55);
  float deep_furrow = smoothstep(0.86, 1.0, abs(sin(furrow_wave * 0.5 + 1.3)));
  float vertical_grain =
      noise2_d(vec2(bark_u * 3.0, bark_v * 25.0 + v_bark_seed * 7.0));
  float bark_noise = noise2_d(vec2(bark_u * 8.0, bark_v * 15.0)) * 0.3;
  float bark_knots = noise2_d(vec2(bark_u * 2.6 + v_leaf_seed * 4.0, bark_v * 9.0));
  float bark_texture =
      furrows * 0.46 + vertical_grain * 0.32 + bark_noise + bark_knots * 0.18;
  bark_texture = mix(bark_texture, 0.0, deep_furrow * 0.55);

  vec3 bark_dark = vec3(0.24, 0.21, 0.18);
  vec3 bark_mid = vec3(0.46, 0.43, 0.38);
  vec3 bark_light = vec3(0.60, 0.58, 0.52);
  vec3 bark_lichen = vec3(0.34, 0.38, 0.30);

  vec3 bark_color = mix(bark_dark, bark_mid, bark_texture);
  float bark_highlight =
      smoothstep(0.72, 0.95, soi_hash_15a407(vec2(bark_v * 15.0, bark_u * 3.0)));
  bark_color = mix(bark_color, bark_light, bark_highlight * 0.48);
  bark_color = mix(bark_color, bark_lichen, smoothstep(0.72, 0.96, bark_knots) * 0.14);
  float basal_lichen = (1.0 - smoothstep(0.04, 0.30, v_local_pos.y)) *
                       smoothstep(0.50, 0.84, vertical_grain);
  bark_color = mix(bark_color, bark_lichen, basal_lichen * 0.50);
  bark_color *= mix(0.72, 1.0, smoothstep(0.0, 0.14, v_local_pos.y));

  vec3 base_color = mix(bark_color, leaf_color, v_foliage_mask);
  vec3 color = base_color * illumination * ao * environment_exposure();

  float translucency =
      backlight * backlight * v_foliage_mask * (0.16 + canopy_edge * 0.34);
  color += leaf_color * vec3(0.48, 0.56, 0.26) * translucency * sun;

  vec3 half_dir = normalize(l + view_dir);
  float leaf_spec =
      pow(max(dot(shading_normal, half_dir), 0.0), 16.0) * v_foliage_mask * 0.060;
  float rim = pow(1.0 - max(dot(geometric_normal, view_dir), 0.0), 4.0) *
              mix(0.055, 0.110, v_foliage_mask);
  color += sun * leaf_spec;
  color += sky * rim;

  if (v_tex_coord.y < 0.035)
    discard;

  color = apply_directional_shadow(color, v_world_pos, geometric_normal);
  color += base_color * ao * local_lighting(v_world_pos, n);
  color = apply_visibility_world_shading(color, v_world_pos.xz);
  frag_color = vec4(color, 1.0);
}
