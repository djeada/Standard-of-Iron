const float k_soi_grade_exposure = 2.15;
const float k_soi_grade_white_point = 3.70;
const float k_soi_grade_contrast = 1.18;
const float k_soi_grade_pivot = 0.38;
const float k_soi_grade_saturation = 1.20;
const vec3 k_soi_grade_shadow_lift = vec3(0.017, 0.022, 0.040);
const vec3 k_soi_grade_highlight_tint = vec3(1.035, 0.990, 0.915);
const vec3 k_soi_grade_shadow_tone = vec3(0.86, 0.94, 1.14);
const vec3 k_soi_grade_highlight_tone = vec3(1.08, 1.005, 0.88);
const float k_soi_grade_split_strength = 0.64;
const float k_soi_grade_split_balance = 0.36;
const vec3 k_soi_grade_luma = vec3(0.2126, 0.7152, 0.0722);

const float k_soi_filmic_a = 0.22;
const float k_soi_filmic_b = 0.30;
const float k_soi_filmic_c = 0.10;
const float k_soi_filmic_d = 0.20;
const float k_soi_filmic_e = 0.01;
const float k_soi_filmic_f = 0.30;

vec3 soi_filmic_curve(vec3 x) {
  return ((x * (k_soi_filmic_a * x + k_soi_filmic_c * k_soi_filmic_b) +
           k_soi_filmic_d * k_soi_filmic_e) /
          (x * (k_soi_filmic_a * x + k_soi_filmic_b) +
           k_soi_filmic_d * k_soi_filmic_f)) -
         k_soi_filmic_e / k_soi_filmic_f;
}

vec3 soi_tonemap(vec3 linear_color) {
  vec3 exposed = max(linear_color, vec3(0.0)) * k_soi_grade_exposure;
  vec3 white = soi_filmic_curve(vec3(k_soi_grade_white_point));
  return soi_filmic_curve(exposed) / white;
}

vec3 soi_split_tone(vec3 color, float strength) {
  float luma = clamp(dot(color, k_soi_grade_luma), 0.0, 1.0);
  float highlight_weight = smoothstep(k_soi_grade_split_balance, 1.0, luma);
  float shadow_weight = 1.0 - smoothstep(0.0, k_soi_grade_split_balance * 1.6, luma);
  vec3 tone = mix(vec3(1.0), k_soi_grade_shadow_tone, shadow_weight * strength);
  tone = mix(tone, k_soi_grade_highlight_tone, highlight_weight * strength);
  return color * tone;
}

vec3 soi_grade(vec3 mapped_color) {
  vec3 contrasted =
      (mapped_color - k_soi_grade_pivot) * k_soi_grade_contrast + k_soi_grade_pivot;
  contrasted = max(contrasted, vec3(0.0));
  float luma = dot(contrasted, k_soi_grade_luma);
  vec3 saturated = mix(vec3(luma), contrasted, k_soi_grade_saturation);
  saturated = max(saturated, vec3(0.0));
  vec3 tinted = mix(saturated, saturated * k_soi_grade_highlight_tint, luma);
  tinted = soi_split_tone(tinted, k_soi_grade_split_strength);
  vec3 lifted =
      k_soi_grade_shadow_lift + tinted * (vec3(1.0) - k_soi_grade_shadow_lift);
  return clamp(lifted, 0.0, 1.0);
}

vec3 soi_finalize(vec3 linear_color) {
  return soi_grade(soi_tonemap(linear_color));
}

const vec3 k_soi_night_shadow_tone = vec3(0.78, 0.84, 1.10);
const vec3 k_soi_night_highlight_tone = vec3(0.98, 1.00, 1.04);
const float k_soi_night_desaturation = 0.18;
const float k_soi_night_black_crush = 0.0025;
const vec3 k_soi_dusk_shadow_tone = vec3(0.94, 0.82, 0.86);
const vec3 k_soi_dusk_highlight_tone = vec3(1.08, 0.94, 0.78);
const float k_soi_dusk_saturation = 0.14;

vec3 soi_time_of_day_grade(vec3 color, float night, float dusk) {
  float luma = clamp(dot(color, k_soi_grade_luma), 0.0, 1.0);
  float shadow_weight = 1.0 - smoothstep(0.0, 0.55, luma);
  float highlight_weight = smoothstep(0.35, 1.0, luma);

  vec3 night_tone = mix(vec3(1.0), k_soi_night_shadow_tone, shadow_weight);
  night_tone = mix(night_tone, k_soi_night_highlight_tone, highlight_weight);
  vec3 night_color = color * night_tone;
  night_color = mix(night_color, vec3(luma) * night_tone, k_soi_night_desaturation);
  night_color = max(night_color - k_soi_night_black_crush, vec3(0.0)) /
                (1.0 - k_soi_night_black_crush);
  color = mix(color, night_color, clamp(night, 0.0, 1.0));

  vec3 dusk_tone = mix(vec3(1.0), k_soi_dusk_shadow_tone, shadow_weight);
  dusk_tone = mix(dusk_tone, k_soi_dusk_highlight_tone, highlight_weight);
  vec3 dusk_color = color * dusk_tone;
  dusk_color = mix(vec3(luma), dusk_color, 1.0 + k_soi_dusk_saturation);
  color = mix(color, max(dusk_color, vec3(0.0)), clamp(dusk, 0.0, 1.0));
  return clamp(color, 0.0, 1.0);
}
