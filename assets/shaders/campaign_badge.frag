#version 330 core

in vec2 v_local;
in vec2 v_uv;

uniform vec4 u_primary_color;
uniform vec4 u_secondary_color;
uniform vec4 u_border_color;
uniform vec4 u_shadow_color;

uniform float u_border_width;
uniform float u_shadow_offset;
uniform float u_time;

uniform int u_badge_style;
uniform bool u_show_shadow;
uniform bool u_animate;

out vec4 frag_color;

float sd_circle(vec2 p, float r) {
  return length(p) - r;
}

float sd_box(vec2 p, vec2 b) {
  vec2 d = abs(p) - b;
  return length(max(d, vec2(0.0))) + min(max(d.x, d.y), 0.0);
}

float sd_shield(vec2 p) {

  vec2 q = abs(p);

  if (p.y < 0.0) {
    float d = sd_box(vec2(q.x, 0.0), vec2(0.4, 0.3));
    return d;
  }

  float r = 0.5;
  float d = length(vec2(q.x, p.y - 0.2)) - r * (1.0 - p.y * 0.5);
  return max(d, p.y - 0.6);
}

float sd_banner(vec2 p) {

  vec2 q = abs(p);

  float rect = sd_box(vec2(q.x, p.y + 0.1), vec2(0.35, 0.5));

  float v_cut = (p.y + 0.4) - q.x * 0.5;

  return max(rect, -v_cut);
}

float sd_medallion(vec2 p) {

  float r = 0.42;
  float d = length(p) - r;

  float angle = atan(p.y, p.x);
  float scallops = sin(angle * 12.0) * 0.03;

  return d + scallops;
}

float sd_seal(vec2 p) {

  float r = 0.4;
  float angle = atan(p.y, p.x);

  float wave1 = sin(angle * 8.0) * 0.04;
  float wave2 = sin(angle * 13.0 + 1.5) * 0.02;
  float wave3 = sin(angle * 21.0 + 3.0) * 0.01;

  float irregularity = wave1 + wave2 + wave3;

  return length(p) - r - irregularity;
}

float sd_standard(vec2 p) {

  vec2 q = abs(p);

  float flag = sd_box(vec2(p.x + 0.1, p.y - 0.1), vec2(0.35, 0.3));

  float pole = sd_box(vec2(p.x + 0.4, p.y), vec2(0.03, 0.5));

  return min(flag, pole);
}

float get_badge_sdf(vec2 p, int style) {
  if (style == 0)
    return sd_standard(p);
  if (style == 1)
    return sd_seal(p);
  if (style == 2)
    return sd_banner(p);
  if (style == 3)
    return sd_shield(p);
  if (style == 4)
    return sd_medallion(p);
  return sd_circle(p, 0.4);
}

float get_emblem_pattern(vec2 p, int style) {
  if (style == 1) {

    float angle = atan(p.y, p.x);
    float r = length(p);

    float leaves = sin(angle * 16.0) * 0.15;
    float ring = abs(r - 0.25) - 0.05;

    return smoothstep(0.02, 0.0, ring + leaves * (r - 0.15));
  }

  if (style == 3 || style == 4) {

    vec2 q = abs(p);
    float cross = min(q.x, q.y) - 0.05;
    return smoothstep(0.02, 0.0, cross) * 0.3;
  }

  return 0.0;
}

void main() {
  vec2 p = v_local;

  if (u_animate) {
    float wobble = sin(u_time * 2.0) * 0.02;
    p.x += wobble * p.y;
  }

  float d = get_badge_sdf(p, u_badge_style);

  float shadow_alpha = 0.0;
  if (u_show_shadow) {
    vec2 shadow_p = p + vec2(u_shadow_offset, -u_shadow_offset);
    float shadow_d = get_badge_sdf(shadow_p, u_badge_style);
    shadow_alpha = smoothstep(0.02, -0.02, shadow_d) * u_shadow_color.a;
  }

  float main_alpha = smoothstep(0.02, -0.02, d);

  float border_d = abs(d) - u_border_width * 0.01;
  float border_alpha = smoothstep(0.01, -0.01, border_d);

  float interior_d = d + u_border_width * 0.01;
  float interior_alpha = smoothstep(0.01, -0.01, interior_d);

  float gradient = (p.y + 0.5) * 0.8;
  vec4 fill_color = mix(u_primary_color, u_secondary_color, gradient);

  float pattern = get_emblem_pattern(p, u_badge_style);
  fill_color.rgb = mix(fill_color.rgb, u_secondary_color.rgb, pattern);

  vec2 light_dir = normalize(vec2(0.5, 0.7));
  float lighting = dot(normalize(vec2(-d, -d)), light_dir) * 0.3 + 0.85;
  fill_color.rgb *= lighting;

  vec4 result = vec4(0.0);

  if (u_show_shadow && shadow_alpha > 0.0) {
    result = mix(result, u_shadow_color, shadow_alpha);
  }

  if (interior_alpha > 0.0) {
    result = mix(result, fill_color, interior_alpha * fill_color.a);
  }

  if (border_alpha > 0.0 && interior_alpha < 0.99) {
    result = mix(result, u_border_color, border_alpha * u_border_color.a);
  }

  result.a = max(result.a, main_alpha * max(fill_color.a, u_border_color.a));
  if (u_show_shadow) {
    result.a = max(result.a, shadow_alpha);
  }

  frag_color = result;
}
