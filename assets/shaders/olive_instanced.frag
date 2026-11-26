#version 330 core
#include "directional_shadows.glsl"
#include "environment_lighting.glsl"
#include "frame_data.glsl"
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
in float v_leaf_tone;
in vec3 v_local_pos;

uniform vec3 u_camera_pos;
uniform float u_snow_coverage;

out vec4 frag_color;

const float k_trunk_base_cut = 0.035;

void main() {
  if (soi_depth_only_pass()) {
    if (v_tex_coord.y < k_trunk_base_cut)
      discard;
    return;
  }
  if (soi_near_camera_keep(v_world_pos, u_camera_pos, gl_FragCoord.xy) < 0.5) {
    discard;
  }

  vec3 geometric_normal = normalize(v_normal);
  vec3 l = environment_primary_direction();
  vec3 view_dir = normalize(u_camera_pos - v_world_pos);

  // Tint entire shoots together so sunlit growth reads at orchard scale.
  // The narrow blades are folded, two-sided sheets with a pale reverse.
  bool blade = v_foliage_mask > 0.5 && v_leaf_tone >= 0.0;
  if (blade && !gl_FrontFacing) {
    geometric_normal = -geometric_normal;
  }
  vec3 seed = vec3(v_leaf_seed * 17.0, v_bark_seed * 11.0, v_leaf_seed * 31.0);
  float canopy_radius = length(v_local_pos.xz);
  float canopy_edge = smoothstep(0.10, 0.50, canopy_radius);
  float canopy_height = smoothstep(0.60, 1.23, v_local_pos.y);
  float shoot = clamp(v_leaf_tone, 0.0, 1.0);
  float leaf_clump = soi_noise3(v_local_pos * 5.0 + seed);
  float leaf_age = smoothstep(0.14, 0.87, shoot * 0.65 + leaf_clump * 0.35);
  vec3 mature = mix(vec3(0.115, 0.265, 0.165), vec3(0.20, 0.34, 0.135), v_leaf_seed);
  vec3 leaf_color = mix(mature, vec3(0.44, 0.53, 0.215), leaf_age);
  // Glancing light picks out the waxy surface between the green shoots.
  float grazing = 1.0 - abs(dot(geometric_normal, view_dir));
  leaf_color = mix(leaf_color, vec3(0.49, 0.57, 0.37), grazing * grazing * 0.42);
  leaf_color *= mix(vec3(1.0), clamp(v_color * 2.8, 0.65, 1.25), 0.18);
  vec3 reverse_color = mix(vec3(0.34, 0.47, 0.345), vec3(0.59, 0.65, 0.50), shoot);
  if (blade && !gl_FrontFacing) {
    leaf_color = reverse_color;
  }
  if (v_leaf_tone < 0.0) {
    leaf_color = vec3(0.32, 0.305, 0.145);
  }
  vec3 n = geometric_normal;
  float backlight = max(-dot(n, l), 0.0);
  vec3 shading_normal = normalize(n + vec3(0.0, 0.48 * v_foliage_mask, 0.0));
  vec3 sun = environment_primary_color() * environment_primary_intensity();
  vec3 sky = environment_sky_color();
  vec3 illumination = environment_ambient_light(shading_normal) +
                      soi_key_light(shading_normal) * mix(0.72, 0.92, v_foliage_mask) +
                      soi_canopy_scatter(shading_normal) * v_foliage_mask * 0.72;
  float ao = mix(1.0, 0.73 + 0.17 * canopy_height + 0.10 * canopy_edge, v_foliage_mask);

  vec3 footprint = fwidth(v_local_pos);
  vec3 bark_color = vec3(0.0);
  if (v_foliage_mask < 0.5) {
    // Local coordinates carry the grain through the forks without a UV seam.
    vec3 grain_pos = v_local_pos * vec3(25.0, 5.0, 25.0);
    float knot = soi_noise3(v_local_pos * 6.0 + seed);
    grain_pos.xz += vec2(2.1, -1.4) * knot;
    float grain = soi_noise3(grain_pos + seed);
    float detail =
        1.0 - smoothstep(0.006, 0.025, max(max(footprint.x, footprint.y), footprint.z));
    float fiber = mix(0.5, soi_noise3(grain_pos * 2.8 + seed), detail);
    bark_color = mix(vec3(0.25, 0.195, 0.14),
                     vec3(0.57, 0.47, 0.36),
                     smoothstep(0.18, 0.83, grain * 0.7 + fiber * 0.3));
    bark_color =
        mix(bark_color, vec3(0.40, 0.405, 0.31), smoothstep(0.48, 0.78, knot) * 0.34);
  }

  vec3 base_color = mix(bark_color, leaf_color, v_foliage_mask);
  float snow_dust =
      clamp(u_snow_coverage, 0.0, 1.0) * v_foliage_mask *
      smoothstep(0.10, 0.70, geometric_normal.y + (leaf_clump - 0.5) * 0.45);
  base_color =
      mix(base_color, vec3(0.86, 0.89, 0.94), clamp(snow_dust * 0.80, 0.0, 0.75));
  vec3 color = base_color * illumination * ao * environment_exposure();

  float translucency =
      backlight * backlight * v_foliage_mask * (0.16 + canopy_edge * 0.34);
  color += leaf_color * vec3(0.48, 0.56, 0.26) * translucency * sun;

  vec3 half_dir = normalize(l + view_dir);
  float leaf_spec =
      pow(max(dot(shading_normal, half_dir), 0.0), 16.0) * v_foliage_mask * 0.060;
  float rim = pow(1.0 - max(dot(geometric_normal, view_dir), 0.0), 4.0) *
              mix(0.055, 0.045, v_foliage_mask);
  color += sun * leaf_spec;

  color += mix(sky, reverse_color * dot(sky, vec3(0.333)) * 1.6, v_foliage_mask) * rim;

  if (v_tex_coord.y < k_trunk_base_cut)
    discard;

  color = apply_directional_shadow(color, v_world_pos, geometric_normal);
  color += base_color * ao * local_lighting(v_world_pos, n);
  color = apply_visibility_world_shading(color, v_world_pos.xz);
  frag_color = vec4(color, 1.0);
}
