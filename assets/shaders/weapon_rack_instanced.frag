#version 330 core

in vec3 v_world_pos;
in vec3 v_normal;
in vec3 v_color;
in vec3 v_local_pos;
in vec3 v_local_normal;

uniform vec3 u_light_direction;

out vec4 frag_color;

float hash12(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float band(float value, float center, float half_width, float feather) {
  return 1.0 - smoothstep(half_width, half_width + feather, abs(value - center));
}

float range_mask(float value, float lo, float hi, float feather) {
  return smoothstep(lo - feather, lo + feather, value) *
         (1.0 - smoothstep(hi - feather, hi + feather, value));
}

void main() {
  vec3 N = normalize(v_normal);
  vec3 LN = normalize(v_local_normal);
  vec3 L = normalize(u_light_direction);
  vec3 V = normalize(vec3(0.0, 0.86, 0.52));
  vec3 H = normalize(L + V);

  float sword_a_t = clamp((v_local_pos.y - 0.42) / 1.40, 0.0, 1.0);
  float sword_b_t = clamp((v_local_pos.y - 0.38) / 1.20, 0.0, 1.0);
  float sword_a_x = mix(-0.250, -0.335, sword_a_t);
  float sword_b_x = mix(0.085, 0.180, sword_b_t);
  float sword_a = band(v_local_pos.x, sword_a_x, 0.080, 0.030) *
                  range_mask(v_local_pos.y, 0.40, 1.96, 0.035) *
                  range_mask(v_local_pos.z, 0.025, 0.185, 0.035);
  float sword_b = band(v_local_pos.x, sword_b_x, 0.068, 0.028) *
                  range_mask(v_local_pos.y, 0.36, 1.68, 0.035) *
                  range_mask(v_local_pos.z, 0.025, 0.190, 0.035);
  float spear_tip_l = range_mask(v_local_pos.x, -0.62, -0.36, 0.035) *
                      range_mask(v_local_pos.y, 1.58, 2.08, 0.035) *
                      range_mask(v_local_pos.z, 0.02, 0.17, 0.035);
  float spear_tip_r = range_mask(v_local_pos.x, 0.54, 0.82, 0.035) *
                      range_mask(v_local_pos.y, 1.56, 2.06, 0.035) *
                      range_mask(v_local_pos.z, 0.01, 0.17, 0.035);
  float blade_facet =
      1.0 - smoothstep(0.93, 0.995, max(abs(LN.x), max(abs(LN.y), abs(LN.z))));
  float blade_mask = clamp(max(max(sword_a, sword_b), max(spear_tip_l, spear_tip_r)) *
                               mix(0.72, 1.0, blade_facet),
                           0.0,
                           1.0);

  float guard_a = band(v_local_pos.y, 0.39, 0.065, 0.025) *
                  range_mask(v_local_pos.x, -0.55, 0.05, 0.035) *
                  range_mask(v_local_pos.z, 0.07, 0.21, 0.025);
  float guard_b = band(v_local_pos.y, 0.34, 0.055, 0.025) *
                  range_mask(v_local_pos.x, -0.11, 0.31, 0.035) *
                  range_mask(v_local_pos.z, 0.08, 0.21, 0.025);
  float brass_mask = max(guard_a, guard_b) * (1.0 - blade_mask);

  float grip_a = range_mask(v_local_pos.x, -0.36, -0.18, 0.025) *
                 range_mask(v_local_pos.y, 0.05, 0.38, 0.025) *
                 range_mask(v_local_pos.z, 0.07, 0.25, 0.025);
  float grip_b = range_mask(v_local_pos.x, 0.02, 0.16, 0.025) *
                 range_mask(v_local_pos.y, 0.05, 0.34, 0.025) *
                 range_mask(v_local_pos.z, 0.08, 0.26, 0.025);
  float leather_mask = max(grip_a, grip_b) * (1.0 - brass_mask);

  float bow_mask = range_mask(v_local_pos.x, 0.18, 0.66, 0.035) *
                   range_mask(v_local_pos.y, 0.04, 1.98, 0.035) *
                   range_mask(v_local_pos.z, -0.14, 0.04, 0.035);

  float wood_grain =
      0.5 + 0.5 * sin(v_local_pos.y * 29.0 + v_local_pos.x * 8.0 + v_local_pos.z * 5.0);
  float wood_mottle = hash12(floor(v_local_pos.xy * 13.0) + v_world_pos.xz * 0.2);
  vec3 dark_oak = v_color * vec3(0.58, 0.54, 0.48);
  vec3 warm_oak = v_color * vec3(1.34, 1.13, 0.78);
  vec3 timber = mix(dark_oak, warm_oak, wood_grain * 0.68 + wood_mottle * 0.32);
  float nick = smoothstep(
      0.88, 0.97, hash12(floor(v_local_pos.zy * 24.0) + floor(v_local_pos.xy * 7.0)));
  timber *= mix(1.0, 0.64, nick * 0.38);

  vec3 yew = mix(vec3(0.30, 0.12, 0.045), vec3(0.66, 0.31, 0.08), wood_grain);
  vec3 leather = mix(vec3(0.24, 0.055, 0.025), vec3(0.58, 0.17, 0.055), wood_mottle);
  float wrap_lines = band(fract(v_local_pos.y * 22.0), 0.5, 0.12, 0.06);
  leather *= mix(0.70, 1.10, wrap_lines);
  vec3 brass = mix(
      vec3(0.42, 0.22, 0.045), vec3(0.82, 0.58, 0.17), hash12(v_world_pos.xz * 9.0));

  float blade_length = max(sword_a_t, sword_b_t);
  float steel_variation = 0.5 + 0.5 * sin(v_local_pos.y * 47.0 + v_local_pos.x * 9.0);
  vec3 steel_dark = vec3(0.25, 0.30, 0.35);
  vec3 steel_bright = vec3(0.72, 0.80, 0.86);
  vec3 steel =
      mix(steel_dark, steel_bright, steel_variation * 0.36 + blade_facet * 0.64);
  float edge = smoothstep(0.58, 0.94, blade_facet) * blade_length;
  steel = mix(steel, vec3(0.90, 0.93, 0.91), edge * 0.48);
  float rust =
      smoothstep(0.88, 0.97, hash12(floor(v_local_pos.xy * 19.0) + vec2(4.0, 9.0)));
  steel = mix(steel, vec3(0.48, 0.16, 0.045), rust * 0.22);

  vec3 albedo = mix(timber, yew, bow_mask);
  albedo = mix(albedo, leather, leather_mask);
  albedo = mix(albedo, brass, brass_mask);
  albedo = mix(albedo, steel, blade_mask);

  float ndotl = max(dot(N, L), 0.0);
  float hemi = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
  vec3 sky = vec3(0.50, 0.59, 0.70);
  vec3 sun = vec3(1.00, 0.86, 0.67);
  vec3 illumination = sky * (0.18 + hemi * 0.14) + sun * ndotl * 0.75;
  float ao = mix(0.54, 1.0, hemi);
  float metal_mask = max(blade_mask, brass_mask);
  float spec_power = mix(24.0, 76.0, blade_mask);
  float specular = pow(max(dot(N, H), 0.0), spec_power) * mix(0.035, 0.48, metal_mask);
  float rim = pow(1.0 - max(dot(N, V), 0.0), 4.0) * 0.05;

  vec3 color = albedo * illumination * ao;
  color += sun * specular;
  color += sky * rim;
  frag_color = vec4(color, 1.0);
}
