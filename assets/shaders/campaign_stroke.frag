#version 330 core
#include "noise.glsl"

in vec2 v_uv;
in float v_distance;
in vec2 v_world_uv;

uniform vec4 u_color;
uniform float u_stroke_width;
uniform float u_total_length;
uniform float u_dash_length;
uniform float u_gap_length;
uniform float u_time;
uniform float u_animation_speed;

uniform bool u_use_dash_pattern;
uniform bool u_use_animation;
uniform bool u_use_feather;
uniform bool u_use_ink_texture;

out vec4 frag_color;

float ink_texture(vec2 uv, float scale) {
  float n1 = soi_value_noise_6c4de2(uv * scale);
  float n2 = soi_value_noise_6c4de2(uv * scale * 2.0 + vec2(50.0));

  return 0.9 + n1 * 0.08 + n2 * 0.02;
}

float feather_edge(float dist, float width, float feather_amount) {
  float half_width = width * 0.5;
  float edge = abs(dist) - half_width;
  return 1.0 - smoothstep(-feather_amount, feather_amount, edge);
}

float dash_pattern(float distance, float dash_len, float gap_len, float anim_offset) {
  float total_len = dash_len + gap_len;
  float t = mod(distance + anim_offset, total_len);
  return step(t, dash_len);
}

void main() {
  vec4 color = u_color;

  if (u_use_ink_texture) {
    float ink = ink_texture(v_world_uv, 50.0);
    color.rgb *= ink;

    float edge_noise = soi_value_noise_6c4de2(v_world_uv * 200.0);
    color.a *= 0.95 + edge_noise * 0.05;
  }

  if (u_use_dash_pattern) {
    float anim_offset = 0.0;
    if (u_use_animation) {
      anim_offset = u_time * u_animation_speed;
    }

    float dash_mask =
        dash_pattern(v_distance, u_dash_length, u_gap_length, anim_offset);

    float total_len = u_dash_length + u_gap_length;
    float t = mod(v_distance + anim_offset, total_len);
    float dash_fade = smoothstep(0.0, u_dash_length * 0.1, t) *
                      smoothstep(u_dash_length, u_dash_length * 0.9, t);

    color.a *= mix(dash_mask, dash_fade, 0.5);
  }

  if (u_use_feather) {

    float cross_dist = abs(v_uv.y);
    float feather = 1.0 - smoothstep(0.7, 1.0, cross_dist);
    color.a *= feather;
  }

  frag_color = vec4(color.rgb * color.a, color.a);
}
