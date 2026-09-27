#version 330 core
#include "directional_shadows.glsl"
#include "environment_lighting.glsl"
#include "local_lighting.glsl"
#include "noise.glsl"
#include "visibility_mask.glsl"

in vec3 v_world_pos;
in vec3 v_normal;
in vec3 v_color;
in vec3 v_local_pos;
in vec3 v_local_normal;

uniform vec3 u_camera_pos;

out vec4 frag_color;

float band(float value, float center, float half_width, float feather) {
  return 1.0 - smoothstep(half_width, half_width + feather, abs(value - center));
}

float cargo_mask(vec3 p, vec2 center, float radius, float height) {
  float radial = length(p.xz - center);
  return (1.0 - smoothstep(radius + 0.004, radius + 0.016, radial)) *
         smoothstep(0.55, 0.57, p.y) *
         (1.0 - smoothstep(0.556 + height + 0.02, 0.556 + height + 0.04, p.y));
}

float filtered_noise(vec2 p) {
  vec2 footprint = fwidth(p);
  return mix(
      0.5, soi_noise2(p), 1.0 - smoothstep(0.35, 1.2, max(footprint.x, footprint.y)));
}

void main() {
  vec3 N = normalize(v_normal);
  vec3 L = environment_primary_direction();
  vec3 V = normalize(u_camera_pos - v_world_pos);
  vec3 H = normalize(L + V);

  float wheel_x = smoothstep(0.71, 0.755, abs(v_local_pos.x)) *
                  (1.0 - smoothstep(0.90, 0.94, abs(v_local_pos.x)));
  float rear = step(0.0, v_local_pos.z);
  vec2 wheel_center = mix(vec2(0.26, -0.50), vec2(0.34, 0.44), rear);
  float wheel_radius = mix(0.26, 0.34, rear);
  float radial = length(v_local_pos.yz - wheel_center);
  float wheel_mask =
      wheel_x * (1.0 - smoothstep(wheel_radius + 0.01, wheel_radius + 0.045, radial));
  float tyre_mask =
      wheel_mask * smoothstep(wheel_radius * 0.86, wheel_radius * 0.94, radial);
  float hub_mask = wheel_mask * (1.0 - smoothstep(0.055, 0.105, radial));

  float barrel_a = cargo_mask(v_local_pos, vec2(-0.28, -0.14), 0.194, 0.62);
  float barrel_b = cargo_mask(v_local_pos, vec2(0.22, 0.06), 0.179, 0.56);
  float barrel_mask = max(barrel_a, barrel_b);
  float canvas_mask = max(cargo_mask(v_local_pos, vec2(-0.24, 0.36), 0.185, 0.44),
                          max(cargo_mask(v_local_pos, vec2(0.20, 0.44), 0.165, 0.38),
                              cargo_mask(v_local_pos, vec2(0.30, -0.36), 0.150, 0.34)));
  float cargo_core = max(barrel_mask, canvas_mask);
  vec2 grain_uv = abs(v_local_normal.z) > 0.65 ? v_local_pos.xy : v_local_pos.zy;
  float warp = soi_noise2(grain_uv * vec2(2.0, 7.0));
  float phase = grain_uv.y * 45.0 + warp * 2.5;
  float longitudinal_grain =
      mix(0.5, 0.5 + 0.5 * sin(phase), 1.0 - smoothstep(0.5, 3.0, fwidth(phase)));
  float cross_grain = filtered_noise(grain_uv * vec2(7.0, 160.0));
  vec3 dark_wood = v_color * vec3(0.62, 0.58, 0.52);
  vec3 fresh_wood = v_color * vec3(1.28, 1.12, 0.88);
  vec3 wood =
      mix(dark_wood, fresh_wood, longitudinal_grain * 0.62 + cross_grain * 0.38);

  float plank_line = max(band(fract((v_local_pos.z + 0.72) * 4.2), 0.5, 0.025, 0.018),
                         band(fract((v_local_pos.y - 0.30) * 6.0), 0.5, 0.020, 0.016));
  wood *= mix(1.0, 0.62, plank_line * 0.46);

  vec3 wheel_wood =
      mix(vec3(0.23, 0.115, 0.045), vec3(0.52, 0.29, 0.10), longitudinal_grain);
  float corrosion = smoothstep(0.52, 0.78, soi_noise2(v_local_pos.yz * 18.0));
  vec3 iron = mix(vec3(0.18, 0.19, 0.20), vec3(0.27, 0.14, 0.065), corrosion);
  vec2 barrel_center = barrel_a > barrel_b ? vec2(-0.28, -0.14) : vec2(0.22, 0.06);
  float barrel_height = barrel_a > barrel_b ? 0.62 : 0.56;
  vec2 barrel_delta = v_local_pos.xz - barrel_center;
  float angle = atan(barrel_delta.y, barrel_delta.x);
  float stave_phase = angle * (16.0 / 6.2831853);
  float stave_edge = abs(fract(stave_phase + 0.5) - 0.5);
  float stave_aa = max(fwidth(stave_phase), 0.008);
  float stave_seam = 1.0 - smoothstep(0.018, 0.018 + stave_aa, stave_edge);
  float stave_grain = filtered_noise(vec2(angle * 70.0, v_local_pos.y * 8.0));
  vec3 barrel = mix(vec3(0.29, 0.18, 0.085), vec3(0.54, 0.36, 0.17), stave_grain);
  barrel *= 1.0 - stave_seam * 0.30 * (1.0 - abs(v_local_normal.y));
  float barrel_t = (v_local_pos.y - 0.556) / barrel_height;
  float barrel_band = max(band(barrel_t, 0.1275, 0.0275, 0.006),
                          max(band(barrel_t, 0.5275, 0.0275, 0.006),
                              band(barrel_t, 0.9275, 0.0275, 0.006)));
  barrel = mix(barrel, iron, barrel_band);
  vec2 weave_uv = vec2(v_local_pos.x + v_local_pos.z, v_local_pos.y) * 210.0;
  float weave_detail = 1.0 - smoothstep(0.4, 1.3, length(fwidth(weave_uv)));
  float weave = sin(weave_uv.x * 6.2831853) * sin(weave_uv.y * 6.2831853);
  float cloth_stain = soi_noise2(v_local_pos.xz * 12.0 + v_local_pos.yy * 4.0);
  vec3 canvas = mix(vec3(0.39, 0.32, 0.20), vec3(0.66, 0.57, 0.39), cloth_stain);
  canvas *= 1.0 + weave * weave_detail * 0.08;

  vec3 albedo = wood;
  albedo = mix(albedo, wheel_wood, wheel_mask);
  albedo = mix(albedo, iron, max(tyre_mask, hub_mask));
  albedo = mix(albedo, barrel, barrel_mask);
  albedo = mix(albedo, canvas, canvas_mask);

  float mud_height = 1.0 - smoothstep(0.04, 0.30, v_local_pos.y);
  float mud_noise = soi_noise2(v_local_pos.xz * 17.0 + v_local_pos.yy * 5.0);
  float mud = mud_height * smoothstep(0.34, 0.78, mud_noise);
  albedo = mix(albedo, vec3(0.24, 0.17, 0.105), mud * 0.58);

  float ndotl = max(dot(N, L), 0.0);
  float hemi = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
  vec3 sky = environment_sky_color();
  vec3 sun = environment_primary_color() * environment_primary_intensity();
  vec3 illumination = soi_surface_lighting_scaled(N, 0.74);
  float cavity = mix(0.56, 1.0, hemi) * mix(1.0, 0.78, cargo_core);
  float metal_mask = max(max(tyre_mask, hub_mask), barrel_band * barrel_mask) *
                     (1.0 - canvas_mask) * (1.0 - mud * 0.8);
  float specular = mix(pow(max(dot(N, H), 0.0), 24.0) * 0.035,
                       pow(max(dot(N, H), 0.0), 54.0) * 0.32,
                       metal_mask);
  float rim = pow(1.0 - max(dot(N, V), 0.0), 4.0) * 0.045;

  vec3 color = albedo * illumination * cavity;
  color += sun * specular;
  color += sky * rim;
  color = apply_directional_shadow(color, v_world_pos, v_normal);
  color += albedo * cavity * local_lighting(v_world_pos, normalize(v_normal));
  color = apply_visibility_revealed(color, v_world_pos.xz);
  frag_color = vec4(color, 1.0);
}
