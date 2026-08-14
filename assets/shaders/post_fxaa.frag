#version 330 core

in vec2 v_uv;

uniform sampler2D u_source;
uniform vec2 u_inverse_resolution;

out vec4 frag_color;

const float k_edge_threshold_min = 0.0312;
const float k_edge_threshold_max = 0.125;
const float k_subpixel_blend = 0.75;
const vec3 k_fxaa_luma = vec3(0.2126, 0.7152, 0.0722);

float sample_luma(vec2 uv) {
  return dot(texture(u_source, uv).rgb, k_fxaa_luma);
}

void main() {
  vec3 center_color = texture(u_source, v_uv).rgb;
  float luma_center = dot(center_color, k_fxaa_luma);
  float luma_north = sample_luma(v_uv + vec2(0.0, u_inverse_resolution.y));
  float luma_south = sample_luma(v_uv - vec2(0.0, u_inverse_resolution.y));
  float luma_east = sample_luma(v_uv + vec2(u_inverse_resolution.x, 0.0));
  float luma_west = sample_luma(v_uv - vec2(u_inverse_resolution.x, 0.0));

  float luma_min =
      min(luma_center, min(min(luma_north, luma_south), min(luma_east, luma_west)));
  float luma_max =
      max(luma_center, max(max(luma_north, luma_south), max(luma_east, luma_west)));
  float range = luma_max - luma_min;

  if (range < max(k_edge_threshold_min, luma_max * k_edge_threshold_max)) {
    frag_color = vec4(center_color, 1.0);
    return;
  }

  float luma_north_west =
      sample_luma(v_uv + vec2(-u_inverse_resolution.x, u_inverse_resolution.y));
  float luma_north_east = sample_luma(v_uv + u_inverse_resolution);
  float luma_south_west = sample_luma(v_uv - u_inverse_resolution);
  float luma_south_east =
      sample_luma(v_uv + vec2(u_inverse_resolution.x, -u_inverse_resolution.y));

  float edge_horizontal =
      abs(luma_north_west + luma_north_east - 2.0 * luma_north) * 2.0 +
      abs(luma_west + luma_east - 2.0 * luma_center) * 4.0 +
      abs(luma_south_west + luma_south_east - 2.0 * luma_south) * 2.0;
  float edge_vertical = abs(luma_north_west + luma_south_west - 2.0 * luma_west) * 2.0 +
                        abs(luma_north + luma_south - 2.0 * luma_center) * 4.0 +
                        abs(luma_north_east + luma_south_east - 2.0 * luma_east) * 2.0;
  bool horizontal_span = edge_horizontal >= edge_vertical;

  vec2 step_direction = horizontal_span ? vec2(0.0, u_inverse_resolution.y)
                                        : vec2(u_inverse_resolution.x, 0.0);
  float luma_positive = horizontal_span ? luma_north : luma_east;
  float luma_negative = horizontal_span ? luma_south : luma_west;
  if (abs(luma_negative - luma_center) > abs(luma_positive - luma_center)) {
    step_direction = -step_direction;
  }

  vec3 blended = 0.5 * (center_color + texture(u_source, v_uv + step_direction).rgb);
  float average = (luma_north + luma_south + luma_east + luma_west) * 0.25;
  float subpixel = clamp(abs(average - luma_center) / max(range, 1e-4), 0.0, 1.0);
  subpixel = smoothstep(0.0, 1.0, subpixel) * k_subpixel_blend;

  frag_color = vec4(mix(center_color, blended, subpixel + 0.5), 1.0);
}
