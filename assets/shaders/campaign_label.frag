#version 330 core

in vec2 v_uv;
in vec2 v_local;
in vec2 v_world_pos;

uniform sampler2_d u_font_atlas;
uniform vec4 u_fill_color;
uniform vec4 u_stroke_color;
uniform float u_stroke_width;
uniform float u_smoothing;

uniform bool u_use_sdf;
uniform bool u_use_stroke;

out vec4 frag_color;

float sample_sdf(vec2 uv) {

  float dist = texture(u_font_atlas, uv).r;

  return (dist - 0.5) * 2.0;
}

void main() {
  if (u_use_sdf) {

    float dist = sample_sdf(v_uv);

    float alpha = smoothstep(-u_smoothing, u_smoothing, dist);

    vec4 color = u_fill_color;
    color.a *= alpha;

    if (u_use_stroke && u_stroke_width > 0.0) {
      float stroke_dist = dist + u_stroke_width * 0.1;
      float stroke_alpha = smoothstep(-u_smoothing, u_smoothing, stroke_dist);

      vec4 stroke_color = u_stroke_color;
      stroke_color.a *= stroke_alpha;

      color = mix(stroke_color, color, alpha);
      color.a = max(stroke_color.a, color.a * alpha);
    }

    frag_color = color;
  } else {

    vec4 tex_color = texture(u_font_atlas, v_uv);

    vec4 color = u_fill_color;
    color.a *= tex_color.a;

    if (u_use_stroke && u_stroke_width > 0.0) {
      float stroke_offset = u_stroke_width * 0.002;

      float neighbors = 0.0;
      neighbors += texture(u_font_atlas, v_uv + vec2(stroke_offset, 0.0)).a;
      neighbors += texture(u_font_atlas, v_uv - vec2(stroke_offset, 0.0)).a;
      neighbors += texture(u_font_atlas, v_uv + vec2(0.0, stroke_offset)).a;
      neighbors += texture(u_font_atlas, v_uv - vec2(0.0, stroke_offset)).a;
      neighbors *= 0.25;

      vec4 stroke_color = u_stroke_color;
      stroke_color.a *= neighbors;

      color = mix(stroke_color, color, tex_color.a);
      color.a = max(stroke_color.a, color.a);
    }

    frag_color = color;
  }

  if (frag_color.a < 0.01) {
    discard;
  }
}
