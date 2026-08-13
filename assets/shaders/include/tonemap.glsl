const float k_soi_grade_exposure = 2.05;
const float k_soi_grade_white_point = 2.80;
const float k_soi_grade_contrast = 1.12;
const float k_soi_grade_pivot = 0.42;
const float k_soi_grade_saturation = 1.15;
const vec3 k_soi_grade_shadow_lift = vec3(0.018, 0.028, 0.046);
const vec3 k_soi_grade_highlight_tint = vec3(1.020, 0.994, 0.940);
const vec3 k_soi_grade_luma = vec3(0.2126, 0.7152, 0.0722);

vec3 soi_tonemap(vec3 linear_color) {
  vec3 exposed = max(linear_color, vec3(0.0)) * k_soi_grade_exposure;
  float white_squared = k_soi_grade_white_point * k_soi_grade_white_point;
  vec3 numerator = exposed * (vec3(1.0) + exposed / white_squared);
  return numerator / (vec3(1.0) + exposed);
}

vec3 soi_grade(vec3 mapped_color) {
  vec3 contrasted =
      (mapped_color - k_soi_grade_pivot) * k_soi_grade_contrast + k_soi_grade_pivot;
  contrasted = max(contrasted, vec3(0.0));
  float luma = dot(contrasted, k_soi_grade_luma);
  vec3 saturated = mix(vec3(luma), contrasted, k_soi_grade_saturation);
  saturated = max(saturated, vec3(0.0));
  vec3 tinted = mix(saturated, saturated * k_soi_grade_highlight_tint, luma);
  vec3 lifted =
      k_soi_grade_shadow_lift + tinted * (vec3(1.0) - k_soi_grade_shadow_lift);
  return clamp(lifted, 0.0, 1.0);
}

vec3 soi_finalize(vec3 linear_color) {
  return soi_grade(soi_tonemap(linear_color));
}
