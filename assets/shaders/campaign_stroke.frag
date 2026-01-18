#version 330 core

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

out vec4 fragColor;

float hash(vec2 p) {
  return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453123);
}

float valueNoise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  vec2 u = f * f * (3.0 - 2.0 * f);

  float a = hash(i);
  float b = hash(i + vec2(1.0, 0.0));
  float c = hash(i + vec2(0.0, 1.0));
  float d = hash(i + vec2(1.0, 1.0));

  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float inkTexture(vec2 uv, float scale) {
  float n1 = valueNoise(uv * scale);
  float n2 = valueNoise(uv * scale * 2.0 + vec2(50.0));

  return 0.9 + n1 * 0.08 + n2 * 0.02;
}

float featherEdge(float dist, float width, float featherAmount) {
  float halfWidth = width * 0.5;
  float edge = abs(dist) - halfWidth;
  return 1.0 - smoothstep(-featherAmount, featherAmount, edge);
}

float dashPattern(float distance, float dashLen, float gapLen,
                  float animOffset) {
  float totalLen = dashLen + gapLen;
  float t = mod(distance + animOffset, totalLen);
  return step(t, dashLen);
}

void main() {
  vec4 color = u_color;

  if (u_use_ink_texture) {
    float ink = inkTexture(v_world_uv, 50.0);
    color.rgb *= ink;

    float edgeNoise = valueNoise(v_world_uv * 200.0);
    color.a *= 0.95 + edgeNoise * 0.05;
  }

  if (u_use_dash_pattern) {
    float animOffset = 0.0;
    if (u_use_animation) {
      animOffset = u_time * u_animation_speed;
    }

    float dashMask =
        dashPattern(v_distance, u_dash_length, u_gap_length, animOffset);

    float totalLen = u_dash_length + u_gap_length;
    float t = mod(v_distance + animOffset, totalLen);
    float dashFade = smoothstep(0.0, u_dash_length * 0.1, t) *
                     smoothstep(u_dash_length, u_dash_length * 0.9, t);

    color.a *= mix(dashMask, dashFade, 0.5);
  }

  if (u_use_feather) {

    float crossDist = abs(v_uv.y);
    float feather = 1.0 - smoothstep(0.7, 1.0, crossDist);
    color.a *= feather;
  }

  fragColor = vec4(color.rgb * color.a, color.a);
}
