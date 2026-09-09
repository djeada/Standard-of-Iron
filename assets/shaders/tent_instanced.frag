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
flat in float v_material;
flat in vec2 v_rot;
flat in vec2 v_anchor;

uniform vec3 u_camera_pos;
uniform float u_time;

out vec4 frag_color;

const float k_canvas_threshold = 0.91;
const float k_lining_threshold = 0.73;
const float k_wood_threshold = 0.55;
const float k_rope_threshold = 0.37;

const float k_half_depth = 0.58;
const float k_ridge_height = 0.86;
const float k_strip_width = 0.30;

const vec3 k_natural_canvas = vec3(0.70, 0.63, 0.49);
const vec3 k_mud = vec3(0.24, 0.18, 0.12);
const vec3 k_lantern_color = vec3(1.0, 0.60, 0.26);
const float k_lantern_gain = 0.85;

float band(float value, float center, float half_width, float feather) {
  return 1.0 - smoothstep(half_width, half_width + feather, abs(value - center));
}

vec3 rotate_to_world(vec3 v) {
  mat2 rot = mat2(v_rot.x, -v_rot.y, v_rot.y, v_rot.x);
  vec2 xz = rot * v.xz;
  return vec3(xz.x, v.y, xz.y);
}

float crease_height(vec3 p, float detail) {
  return soi_noise3(p * 9.0 + 3.7) * 0.7 + soi_noise3(p * 26.0) * 0.3 * detail;
}

vec3 crease_normal(vec3 local_n, float detail) {
  const float e = 0.004;
  vec3 p = v_local_pos;
  float h0 = crease_height(p, detail);
  vec3 g = vec3(crease_height(p + vec3(e, 0.0, 0.0), detail) - h0,
                crease_height(p + vec3(0.0, e, 0.0), detail) - h0,
                crease_height(p + vec3(0.0, 0.0, e), detail) - h0) /
           e;
  g -= local_n * dot(g, local_n);
  return normalize(local_n - g * 0.035);
}

void main() {
  float is_canvas = step(k_canvas_threshold, v_material);
  float is_lining = step(k_lining_threshold, v_material) - is_canvas;
  float is_wood =
      step(k_wood_threshold, v_material) - step(k_lining_threshold, v_material);
  float is_rope =
      step(k_rope_threshold, v_material) - step(k_wood_threshold, v_material);
  float is_floor = 1.0 - step(k_rope_threshold, v_material);
  float cloth = is_canvas + is_lining;

  vec3 L = environment_primary_direction();
  vec3 V = normalize(u_camera_pos - v_world_pos);
  float view_dist = distance(u_camera_pos, v_world_pos);
  float detail = 1.0 - smoothstep(16.0, 38.0, view_dist);
  float mid_detail = 1.0 - smoothstep(30.0, 70.0, view_dist);

  vec3 p = v_local_pos;
  vec3 local_n = normalize(v_local_normal);
  vec3 N = normalize(v_normal);
  if (cloth > 0.5) {
    N = rotate_to_world(crease_normal(local_n, detail));
  }

  float end_wall = smoothstep(0.55, 0.85, abs(local_n.z));
  float roof = smoothstep(0.35, 0.60, abs(local_n.y));
  vec2 uv = mix(vec2(p.z, p.y * 1.25 + abs(p.x) * 1.05), p.xy, end_wall);

  float pick = soi_hash12_9f6e8e(v_anchor * 0.731 + 17.3);
  float striped = step(0.42, pick) * (1.0 - step(0.76, pick));
  float trimmed = step(0.76, pick);
  float strip_index = floor(uv.x / k_strip_width + 0.5);
  float strip_parity = mod(strip_index, 2.0);
  vec3 dye = v_color;
  vec3 natural = k_natural_canvas * mix(0.94, 1.06, soi_hash12_9f6e8e(v_anchor + 4.2));

  float awning = step(0.30, p.y) * step(0.62, -p.z);
  float door_frame = end_wall * step(0.40, -p.z) * (1.0 - step(0.20, abs(p.x))) *
                     (1.0 - step(0.57, p.y)) *
                     clamp(step(0.13, abs(p.x)) + step(0.50, p.y), 0.0, 1.0);
  float trim = clamp(roof * band(p.y, 0.215, 0.045, 0.02) + awning * step(0.77, -p.z) +
                         door_frame,
                     0.0,
                     1.0);

  vec3 body = mix(dye, natural, trimmed);
  body = mix(body, mix(dye, natural, strip_parity), striped);
  body = mix(body, dye, trimmed * trim);

  float weave = (0.5 + 0.5 * sin(uv.x * 310.0)) * (0.5 + 0.5 * sin(uv.y * 300.0));
  weave = mix(0.5, weave, detail);
  float mottle = soi_fbm_23e5ab(uv * 7.0 + v_anchor * 0.31);
  float stain = soi_fbm_23e5ab(uv * 2.6 + 9.0 + v_anchor * 0.13);
  vec3 canvas = body * mix(0.80, 1.14, mottle * 0.65 + weave * 0.35);
  float damp = (1.0 - smoothstep(0.02, 0.26, p.y)) * (0.4 + 0.6 * stain);
  canvas = mix(canvas, canvas * vec3(0.58, 0.56, 0.48), damp * 0.55);
  float mud = (1.0 - smoothstep(0.0, 0.10, p.y)) *
              (0.5 + 0.5 * soi_hash12_9f6e8e(floor(uv * 40.0)));
  canvas = mix(canvas, k_mud, mud * 0.6);
  float bleach =
      smoothstep(0.45, k_ridge_height, p.y) * smoothstep(0.2, 0.8, abs(local_n.y));
  canvas = mix(canvas, canvas * 0.55 + vec3(0.36, 0.33, 0.27), bleach * 0.30);

  float seam_pos = abs(fract(uv.x / k_strip_width + 0.5) - 0.5) * k_strip_width;
  float seam =
      band(seam_pos, 0.0, 0.004, 0.006) + band(seam_pos, 0.014, 0.003, 0.005) * 0.7;
  float hem = roof * band(p.y, 0.185, 0.010, 0.008) +
              (1.0 - roof) * (1.0 - end_wall) * band(p.y, 0.195, 0.008, 0.006);
  float ridge_seam = roof * band(p.x, 0.0, 0.012, 0.012) * smoothstep(0.70, 0.85, p.y);
  float stitch = clamp(seam + hem + ridge_seam, 0.0, 1.0) * mid_detail;
  canvas = mix(canvas, canvas * vec3(0.66, 0.62, 0.56), stitch * 0.55);
  vec2 patch_center =
      vec2((floor(soi_hash12_9f6e8e(v_anchor + 8.8) * 6.0) - 2.5) * k_strip_width,
           0.40 + soi_hash12_9f6e8e(v_anchor + 2.1) * 0.25);
  vec2 patch_d = abs(uv - patch_center) - vec2(0.075, 0.055);
  float patch = (1.0 - step(0.0, max(patch_d.x, patch_d.y))) * (1.0 - end_wall) *
                (1.0 - awning) * mid_detail;
  float patch_edge = patch * step(-0.012, max(patch_d.x, patch_d.y));
  canvas = mix(canvas, natural * mix(0.84, 0.96, mottle), patch * 0.85);
  canvas = mix(canvas, canvas * 0.7, patch_edge * 0.6);

  vec3 lining = canvas * vec3(0.80, 0.78, 0.74);

  float grain = 0.5 + 0.5 * sin(p.y * 70.0 + p.z * 70.0 + p.x * 9.0 +
                                soi_fbm_23e5ab(p.xz * 20.0 + p.y * 3.0) * 4.0);
  vec3 wood = mix(vec3(0.30, 0.22, 0.14),
                  vec3(0.56, 0.44, 0.30),
                  grain * 0.7 + 0.3 * soi_hash12_9f6e8e(floor(p.xz * 30.0)));
  wood =
      mix(wood, vec3(0.42, 0.40, 0.36), smoothstep(0.30, k_ridge_height, p.y) * 0.35);
  wood = mix(wood, k_mud, (1.0 - smoothstep(0.0, 0.08, p.y)) * 0.5);

  float twist = 0.5 + 0.5 * sin((p.x + p.y + p.z) * 260.0);
  vec3 rope =
      mix(vec3(0.44, 0.36, 0.24), vec3(0.68, 0.58, 0.40), mix(0.5, twist, detail));
  rope = mix(rope, k_mud, (1.0 - smoothstep(0.0, 0.06, p.y)) * 0.4);

  float rush = band(fract(p.x * 26.0), 0.5, 0.20, 0.15);
  vec3 matting = mix(vec3(0.33, 0.26, 0.16),
                     vec3(0.50, 0.42, 0.26),
                     soi_fbm_23e5ab(p.xz * 9.0) * 0.7 + rush * 0.3 * detail);

  vec3 albedo = canvas * is_canvas + lining * is_lining + wood * is_wood +
                rope * is_rope + matting * is_floor;

  vec3 sun = environment_primary_color() * environment_primary_intensity();
  vec3 sky = environment_sky_color();
  float exposure = environment_exposure();
  float hemi = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
  float under_awning = (1.0 - smoothstep(0.50, 0.62, p.y)) *
                       smoothstep(-0.03, 0.02, -(p.z + k_half_depth)) *
                       (1.0 - step(0.44, abs(p.x)));
  float foot_ao = 1.0 - (1.0 - smoothstep(0.0, 0.12, p.y)) * 0.35;
  float ao = mix(0.60, 1.0, hemi) * foot_ao * (1.0 - under_awning * 0.45);

  float inside_box = (1.0 - step(0.46, abs(p.x))) * (1.0 - step(0.565, abs(p.z))) *
                     (1.0 - step(0.845, p.y));
  float interior = clamp(is_lining + is_floor + is_wood * inside_box, 0.0, 1.0);
  float door_near = smoothstep(0.10, 0.58, -p.z);
  vec3 outdoor_lit = soi_surface_lighting_scaled(N, 0.9);
  vec3 filtered = sun * 0.16 * (0.5 + 0.5 * clamp(dot(-N, L), 0.0, 1.0)) * is_lining;
  vec3 indoor_lit =
      (environment_ambient_light(vec3(0.0, 1.0, 0.0)) * (0.35 + 0.35 * door_near) +
       filtered) *
      exposure;
  vec3 lit = mix(outdoor_lit, indoor_lit, interior);

  vec3 color = albedo * lit * ao;

  float backlit = clamp(dot(-N, L), 0.0, 1.0) * is_canvas * (1.0 - mud);
  color += albedo * sun * backlit * 0.22 * exposure;

  vec3 Hh = normalize(L + V);
  float spec = pow(max(dot(N, Hh), 0.0), mix(14.0, 40.0, is_wood)) *
               (0.035 * cloth + 0.06 * is_wood + 0.02 * is_rope);
  color += sun * spec * (1.0 - interior);
  color += sky * pow(1.0 - max(dot(N, V), 0.0), 4.0) * 0.04 * (1.0 - interior);

  color = apply_directional_shadow(color, v_world_pos, N);
  color += albedo * ao * local_lighting(v_world_pos, N);

  float night = environment_night_amount();
  if (night > 0.0) {
    vec3 lantern_local = vec3(0.0, 0.42, -0.06);
    float d = length(p - lantern_local);
    float fall = 1.0 / (1.0 + 10.0 * d * d);
    float seed = soi_hash12_9f6e8e(v_anchor) * 6.2831;
    float flicker =
        0.90 + 0.10 * sin(u_time * 7.3 + seed) * sin(u_time * 3.1 + seed * 0.5);
    float body_canvas = is_canvas * (1.0 - awning);
    vec3 through_cloth = albedo * 1.6 * body_canvas * (1.0 - mud * 0.8) *
                         (1.0 - stitch * 0.4) * (0.94 + 0.06 * weave);
    float inner =
        is_lining * (0.92 + 0.08 * weave) + is_floor * 0.8 + is_wood * inside_box * 0.6;
    color += k_lantern_color * (through_cloth + vec3(inner)) * fall * flicker * night *
             k_lantern_gain;
  }

  color = apply_visibility_revealed(color, v_world_pos.xz);
  frag_color = vec4(color, 1.0);
}
