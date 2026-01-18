#version 330 core

in vec2 v_uv;
in vec2 v_local;
in vec2 v_world_pos;

uniform sampler2D u_font_atlas;
uniform vec4 u_fill_color;
uniform vec4 u_stroke_color;
uniform float u_stroke_width;
uniform float u_smoothing;

uniform bool u_use_sdf;
uniform bool u_use_stroke;

out vec4 fragColor;

float sampleSDF(vec2 uv) {

  float dist = texture(u_font_atlas, uv).r;

  return (dist - 0.5) * 2.0;
}

void main() {
  if (u_use_sdf) {

    float dist = sampleSDF(v_uv);

    float alpha = smoothstep(-u_smoothing, u_smoothing, dist);

    vec4 color = u_fill_color;
    color.a *= alpha;

    if (u_use_stroke && u_stroke_width > 0.0) {
      float strokeDist = dist + u_stroke_width * 0.1;
      float strokeAlpha = smoothstep(-u_smoothing, u_smoothing, strokeDist);

      vec4 strokeColor = u_stroke_color;
      strokeColor.a *= strokeAlpha;

      color = mix(strokeColor, color, alpha);
      color.a = max(strokeColor.a, color.a * alpha);
    }

    fragColor = color;
  } else {

    vec4 texColor = texture(u_font_atlas, v_uv);

    vec4 color = u_fill_color;
    color.a *= texColor.a;

    if (u_use_stroke && u_stroke_width > 0.0) {
      float strokeOffset = u_stroke_width * 0.002;

      float neighbors = 0.0;
      neighbors += texture(u_font_atlas, v_uv + vec2(strokeOffset, 0.0)).a;
      neighbors += texture(u_font_atlas, v_uv - vec2(strokeOffset, 0.0)).a;
      neighbors += texture(u_font_atlas, v_uv + vec2(0.0, strokeOffset)).a;
      neighbors += texture(u_font_atlas, v_uv - vec2(0.0, strokeOffset)).a;
      neighbors *= 0.25;

      vec4 strokeColor = u_stroke_color;
      strokeColor.a *= neighbors;

      color = mix(strokeColor, color, texColor.a);
      color.a = max(strokeColor.a, color.a);
    }

    fragColor = color;
  }

  if (fragColor.a < 0.01) {
    discard;
  }
}
