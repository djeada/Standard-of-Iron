#version 330 core

in vec2 v_local;
in float v_symbol_type;

uniform vec4 u_fill_color;
uniform vec4 u_stroke_color;
uniform vec4 u_shadow_color;
uniform float u_stroke_width;
uniform float u_shadow_offset;
uniform bool u_show_shadow;

out vec4 frag_color;

const float SYMBOL_MOUNTAIN = 0.0;
const float SYMBOL_CITY = 1.0;
const float SYMBOL_PORT = 2.0;
const float SYMBOL_FORT = 3.0;
const float SYMBOL_TEMPLE = 4.0;

float sd_triangle(vec2 p, vec2 a, vec2 b, vec2 c) {
  vec2 e0 = b - a, e1 = c - b, e2 = a - c;
  vec2 v0 = p - a, v1 = p - b, v2 = p - c;

  vec2 pq0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), 0.0, 1.0);
  vec2 pq1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), 0.0, 1.0);
  vec2 pq2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), 0.0, 1.0);

  float s = sign(e0.x * e2.y - e0.y * e2.x);
  vec2 d = min(min(vec2(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)),
                   vec2(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))),
               vec2(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)));

  return -sqrt(d.x) * sign(d.y);
}

float sd_box(vec2 p, vec2 b) {
  vec2 d = abs(p) - b;
  return length(max(d, vec2(0.0))) + min(max(d.x, d.y), 0.0);
}

float sd_circle(vec2 p, float r) { return length(p) - r; }

float sd_mountain(vec2 p) {

  float peak1 =
      sd_triangle(p, vec2(-0.35, 0.35), vec2(0.0, -0.4), vec2(0.35, 0.35));

  vec2 p2 = p - vec2(0.25, 0.1);
  float peak2 =
      sd_triangle(p2, vec2(-0.2, 0.25), vec2(0.0, -0.2), vec2(0.2, 0.25));

  float snow_line = p.y + 0.15;

  return min(peak1, peak2);
}

float sd_city(vec2 p) {

  float base = sd_box(p - vec2(0.0, 0.15), vec2(0.35, 0.2));

  float tower = sd_box(p - vec2(0.0, -0.15), vec2(0.1, 0.35));

  float left = sd_box(p - vec2(-0.2, 0.0), vec2(0.1, 0.25));
  float right = sd_box(p - vec2(0.2, 0.0), vec2(0.1, 0.2));

  return min(min(base, tower), min(left, right));
}

float sd_port(vec2 p) {

  float ring = abs(length(p - vec2(0.0, -0.3)) - 0.1) - 0.03;

  float shaft = sd_box(p, vec2(0.04, 0.3));

  float crossbar = sd_box(p - vec2(0.0, -0.1), vec2(0.2, 0.03));

  vec2 pl = p - vec2(-0.15, 0.2);
  vec2 pr = p - vec2(0.15, 0.2);
  float fluke_l = sd_box(pl, vec2(0.1, 0.04));
  float fluke_r = sd_box(pr, vec2(0.1, 0.04));

  return min(min(min(ring, shaft), crossbar), min(fluke_l, fluke_r));
}

float sd_fort(vec2 p) {

  float wall = sd_box(p - vec2(0.0, 0.1), vec2(0.35, 0.15));

  float t1 = sd_box(p - vec2(-0.3, -0.1), vec2(0.08, 0.2));
  float t2 = sd_box(p - vec2(0.0, -0.15), vec2(0.08, 0.25));
  float t3 = sd_box(p - vec2(0.3, -0.1), vec2(0.08, 0.2));

  float m1 = sd_box(p - vec2(-0.3, -0.35), vec2(0.05, 0.05));
  float m2 = sd_box(p - vec2(0.0, -0.45), vec2(0.05, 0.05));
  float m3 = sd_box(p - vec2(0.3, -0.35), vec2(0.05, 0.05));

  return min(min(min(wall, min(t1, min(t2, t3))), min(m1, min(m2, m3))), wall);
}

float sd_temple(vec2 p) {

  float roof =
      sd_triangle(p, vec2(-0.35, -0.1), vec2(0.0, -0.4), vec2(0.35, -0.1));

  float base = sd_box(p - vec2(0.0, 0.3), vec2(0.38, 0.08));

  float c1 = sd_box(p - vec2(-0.25, 0.1), vec2(0.04, 0.2));
  float c2 = sd_box(p - vec2(-0.08, 0.1), vec2(0.04, 0.2));
  float c3 = sd_box(p - vec2(0.08, 0.1), vec2(0.04, 0.2));
  float c4 = sd_box(p - vec2(0.25, 0.1), vec2(0.04, 0.2));

  return min(min(roof, base), min(min(c1, c2), min(c3, c4)));
}

float get_symbol_sdf(vec2 p, float symbol_type) {
  if (symbol_type < 0.5)
    return sd_mountain(p);
  if (symbol_type < 1.5)
    return sd_city(p);
  if (symbol_type < 2.5)
    return sd_port(p);
  if (symbol_type < 3.5)
    return sd_fort(p);
  return sd_temple(p);
}

void main() {
  vec2 p = v_local;

  float d = get_symbol_sdf(p, v_symbol_type);

  float shadow_alpha = 0.0;
  if (u_show_shadow) {
    vec2 shadow_p = p + vec2(u_shadow_offset, -u_shadow_offset) * 0.05;
    float shadow_d = get_symbol_sdf(shadow_p, v_symbol_type);
    shadow_alpha = smoothstep(0.02, -0.02, shadow_d) * u_shadow_color.a;
  }

  float fill_alpha = smoothstep(0.02, -0.02, d);

  float stroke_d = abs(d) - u_stroke_width * 0.02;
  float stroke_alpha = smoothstep(0.01, -0.01, stroke_d);

  float interior_d = d + u_stroke_width * 0.02;
  float interior_alpha = smoothstep(0.01, -0.01, interior_d);

  vec4 result = vec4(0.0);

  if (shadow_alpha > 0.0) {
    result = mix(result, u_shadow_color, shadow_alpha);
  }

  if (interior_alpha > 0.0) {
    result = mix(result, u_fill_color, interior_alpha * u_fill_color.a);
  }

  if (stroke_alpha > 0.0 && interior_alpha < 0.99) {
    result = mix(result, u_stroke_color, stroke_alpha * u_stroke_color.a);
  }

  result.a = max(result.a, fill_alpha * max(u_fill_color.a, u_stroke_color.a));
  if (u_show_shadow) {
    result.a = max(result.a, shadow_alpha);
  }

  frag_color = result;
}
