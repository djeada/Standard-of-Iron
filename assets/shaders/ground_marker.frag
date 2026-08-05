#version 330 core

in vec2 v_shape_coord;
in vec3 v_color;
in float v_alpha;
in float v_pattern;
in float v_flags;
in float v_phase;

uniform vec4 u_pattern_table[12];
uniform float u_time;

out vec4 frag_color;

const float k_band_inner = 0.0;
const float k_band_outer = 1.0;

float soft_band(float value, float low, float high, float feather) {
  return smoothstep(low - feather, low + feather, value) *
         (1.0 - smoothstep(high - feather, high + feather, value));
}

float dash_gate(float angle, float dash_count, float duty, float feather) {
  if (dash_count <= 1.0 && duty >= 0.999) {
    return 1.0;
  }
  float slot = fract(angle * dash_count);
  return 1.0 - smoothstep(duty - feather, duty + feather, slot);
}

void main() {
  int pattern_index = int(v_pattern + 0.5);
  vec4 spec_a = u_pattern_table[pattern_index * 2];
  vec4 spec_b = u_pattern_table[pattern_index * 2 + 1];

  float dash_count = spec_a.x;
  float dash_duty = spec_a.y;
  float second_start = spec_a.z;
  float second_end = spec_a.w;
  float tick_count = spec_b.x;
  float tick_length = spec_b.y;

  float angle = v_shape_coord.x;
  float radial = v_shape_coord.y;

  float radial_feather = max(fwidth(radial) * 0.75, 0.02);
  float angular_feather = max(fwidth(angle) * dash_count, 0.004);

  float main_band = soft_band(radial, k_band_inner, k_band_outer, radial_feather);
  main_band *= dash_gate(angle, dash_count, dash_duty, angular_feather);

  float coverage = main_band;

  if (second_end > second_start) {
    coverage =
        max(coverage, soft_band(radial, second_start, second_end, radial_feather));
  }

  if (tick_count > 0.0) {
    float tick_slot = abs(fract(angle * tick_count + 0.5) - 0.5) * 2.0;
    float tick_gate = 1.0 - smoothstep(0.12, 0.2, tick_slot);
    float tick_band =
        soft_band(radial, k_band_inner, k_band_outer + tick_length, radial_feather);
    coverage = max(coverage, tick_band * tick_gate);
  }

  float glow_center = (k_band_inner + k_band_outer) * 0.5;
  float glow = exp(-pow(abs(radial - glow_center) / 1.5, 2.0)) * 0.32;

  bool focused = v_flags >= 0.5;
  float pulse = focused ? 0.86 + 0.14 * sin(u_time * 3.6 + v_phase) : 1.0;

  float alpha = (coverage + glow * (focused ? 0.9 : 0.55)) * v_alpha * pulse;
  if (alpha <= 0.004) {
    discard;
  }

  vec3 color = v_color * (1.0 + coverage * (focused ? 0.55 : 0.28));
  frag_color = vec4(color, clamp(alpha, 0.0, 1.0));
}
