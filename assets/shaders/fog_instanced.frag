#version 330 core

in vec3 v_world_pos;
in vec3 v_color;
in float v_alpha;
in vec2 v_local_coord;
in float v_seed;

out vec4 frag_color;

uniform float u_time;

float hash11(float x) {
  return fract(sin(x * 91.345) * 43758.5453);
}

float hash21(vec2 p) {
  p = fract(p * vec2(123.34, 456.21));
  p += dot(p, p + 78.233);
  return fract(p.x * p.y);
}

float vnoise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  vec2 u = f * f * (3.0 - 2.0 * f);

  float a = hash21(i + vec2(0.0, 0.0));
  float b = hash21(i + vec2(1.0, 0.0));
  float c = hash21(i + vec2(0.0, 1.0));
  float d = hash21(i + vec2(1.0, 1.0));

  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

void main() {
  vec2 p = v_local_coord * 2.0;
  vec2 ap = abs(p);
  float outer = max(ap.x, ap.y);
  if (outer >= 1.08) {
    discard;
  }

  vec2 wp = v_world_pos.xz;
  vec2 drift = vec2(u_time * 0.010, -u_time * 0.007);
  float large = vnoise(wp * 0.045 + vec2(v_seed * 7.0, -v_seed * 5.0) + drift);
  float detail =
      vnoise(wp * 0.120 + vec2(13.7, 4.1) + vec2(v_seed * 3.0, v_seed * 9.0));
  float erosion = vnoise(wp * 0.080 + vec2(21.5, -7.4) + drift * 0.6 +
                         vec2(v_seed * 11.0, -v_seed * 6.0));

  float body = smoothstep(0.18, 0.88, large * 0.72 + detail * 0.40);
  float edge_break = mix(0.08, 0.24, erosion);
  float mask = 1.0 - smoothstep(0.68 - edge_break, 1.0, outer);
  mask *= body;
  mask *= 1.0 - smoothstep(0.80, 1.06, length(p));

  if (mask <= 0.002) {
    discard;
  }

  float interior = 1.0 - smoothstep(0.15, 0.90, outer);
  float bright_jitter = mix(0.90, 1.10, hash11(v_seed + 2.71));
  vec3 base = v_color * bright_jitter;
  vec3 lit = mix(base * 0.78, base * 1.08, interior);
  lit *= mix(0.90, 1.08, detail);
  lit *= 0.94 + 0.08 * large;

  float alpha = v_alpha * mask;
  alpha *= mix(0.62, 1.0, body);

  if (alpha <= 0.004) {
    discard;
  }

  frag_color = vec4(lit, clamp(alpha, 0.0, 1.0));
}
