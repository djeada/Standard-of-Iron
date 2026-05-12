#version 330 core

in vec2 v_tex_coord;
in vec3 v_world_pos;
in vec3 v_world_normal;
in float v_sheet_drift;

uniform vec3 u_color;
uniform float u_alpha;
uniform float u_time;

out vec4 frag_color;

float hash(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float a = hash(i);
  float b = hash(i + vec2(1.0, 0.0));
  float c = hash(i + vec2(0.0, 1.0));
  float d = hash(i + vec2(1.0, 1.0));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main() {
  vec2 flow_a = vec2(u_time * 0.018, -u_time * 0.031);
  vec2 flow_b = vec2(-u_time * 0.024, u_time * 0.013);

  float low_freq = noise(v_world_pos.xz * 0.045 + flow_a + v_tex_coord.yy * 0.9);
  float mid_freq = noise(v_world_pos.xz * 0.090 + flow_b +
                        vec2(v_tex_coord.x, v_tex_coord.y * 1.4));
  float vertical_ripples =
      noise(vec2(v_world_pos.y * 0.18 + u_time * 0.06, v_world_pos.x * 0.05));

  float density_field = low_freq * 0.55 + mid_freq * 0.35 + vertical_ripples * 0.10;

  float wisps = smoothstep(0.36, 0.84,
                           density_field + v_tex_coord.y * 0.18 -
                               abs(v_tex_coord.x - 0.5) * 0.12);

  float edge_fade = smoothstep(0.0, 0.12, v_tex_coord.x) *
                   smoothstep(0.0, 0.12, 1.0 - v_tex_coord.x);
  float base_fade = smoothstep(0.0, 0.05, v_tex_coord.y);
  float top_fade = 1.0 - smoothstep(0.62, 1.0, v_tex_coord.y);
  float height_fade = base_fade * (0.65 + 0.35 * top_fade);

  float moving_bands = 0.75 + 0.25 * sin(v_world_pos.y * 0.22 + u_time * 0.70 +
                                        density_field * 4.0);
  float front_light = 0.80 + 0.20 * max(dot(normalize(v_world_normal),
                                           normalize(vec3(0.4, 0.7, 0.5))),
                                       0.0);

  float alpha =
      u_alpha * edge_fade * height_fade * moving_bands * (0.22 + 0.78 * wisps);
  alpha *= 0.82 + 0.18 * smoothstep(-0.25, 0.25, v_sheet_drift);

  vec3 dark_color = u_color * vec3(0.78, 0.80, 0.82);
  vec3 light_color = u_color * vec3(1.18, 1.20, 1.24);
  vec3 color = mix(dark_color, light_color, density_field);
  color *= front_light;
  color += vec3(0.035, 0.040, 0.038) * wisps;

  frag_color = vec4(color, clamp(alpha, 0.0, 0.92));
}
