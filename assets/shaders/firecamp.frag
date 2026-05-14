#version 330 core
out vec4 frag_color;
in vec2 tex_coord;
in float intensity_val;
in float flame_phase;
in float flame_height;

uniform sampler2D fire_texture;
uniform float u_time;
uniform float u_glow_strength;

float hash(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise(vec2 p) {
  vec2 cell = floor(p);
  vec2 local = fract(p);
  vec2 smooth_local = local * local * (3.0 - 2.0 * local);

  float a = hash(cell);
  float b = hash(cell + vec2(1.0, 0.0));
  float c = hash(cell + vec2(0.0, 1.0));
  float d = hash(cell + vec2(1.0, 1.0));

  return mix(mix(a, b, smooth_local.x), mix(c, d, smooth_local.x), smooth_local.y);
}

float fbm(vec2 p) {
  float value = 0.0;
  float amplitude = 0.5;
  for (int octave = 0; octave < 4; ++octave) {
    value += amplitude * noise(p);
    p = p * 2.03 + vec2(17.13, 9.37);
    amplitude *= 0.5;
  }
  return value;
}

void main() {
  float height_t = clamp(flame_height, 0.0, 1.0);
  float intensity_scale = clamp(intensity_val, 0.6, 1.6);
  float centered_x = tex_coord.x * 2.0 - 1.0;

  vec2 animated_uv =
      vec2(tex_coord.x, tex_coord.y + fract(u_time * 0.34 + flame_phase * 0.05));
  vec4 tex_color = texture(fire_texture, animated_uv);
  float tex_detail = dot(clamp(tex_color.rgb, 0.0, 1.0), vec3(0.3333));

  vec2 flow_uv =
      vec2(centered_x * 1.35 + flame_phase * 0.11, tex_coord.y * 3.15 - u_time * 1.85);
  float coarse_noise = fbm(flow_uv);
  float fine_noise = fbm(flow_uv * 1.9 + vec2(3.4, -u_time * 0.35));
  float curl_noise =
      fbm(vec2((centered_x + (coarse_noise - 0.5) * 0.45) * 4.7 + flame_phase * 0.27,
               tex_coord.y * 5.1 - u_time * 3.0));

  float flicker_wave = 0.5 + 0.5 * sin(u_time * 7.2 + flame_phase * 2.6 +
                                       coarse_noise * 3.14159 + height_t * 10.0);
  float flicker =
      clamp(0.82 + flicker_wave * 0.26 + (fine_noise - 0.5) * 0.16, 0.65, 1.35);

  float side_shift =
      (coarse_noise - 0.5) * mix(0.08, 0.46, height_t) + (fine_noise - 0.5) * 0.12;
  float width = mix(0.8, 0.1, pow(height_t, 0.72));
  float inner_width = width * mix(0.56, 0.42, height_t);
  float body_mask = 1.0 - smoothstep(width, width + 0.2, abs(centered_x + side_shift));
  float core_mask =
      1.0 -
      smoothstep(inner_width, inner_width + 0.14, abs(centered_x + side_shift * 0.7));

  float erosion = smoothstep(0.16,
                             0.9,
                             curl_noise + coarse_noise * 0.42 - height_t * 0.34 +
                                 tex_detail * 0.18);
  body_mask *= erosion;
  core_mask *= smoothstep(0.0, 0.9, erosion + 0.12);

  float base_fill = smoothstep(0.0, 0.08, tex_coord.y);
  float top_fade =
      1.0 - smoothstep(0.72, 1.08, tex_coord.y + (fine_noise - 0.5) * 0.16);
  float alpha = body_mask * base_fill * top_fade * tex_color.a * intensity_scale *
                mix(0.78, 1.06, flicker);

  vec3 base_white = vec3(2.2, 1.9, 1.3);
  vec3 hot_core = vec3(1.85, 1.08, 0.3);
  vec3 mid_flame = vec3(1.2, 0.42, 0.08);
  vec3 ember_red = vec3(0.52, 0.12, 0.05);
  vec3 smoke_tip = vec3(0.18, 0.08, 0.07);

  vec3 flame = mix(mid_flame, ember_red, smoothstep(0.55, 1.0, height_t));
  flame = mix(flame, hot_core, core_mask * (0.52 + 0.18 * flicker));
  flame =
      mix(flame, base_white, core_mask * (1.0 - smoothstep(0.0, 0.22, height_t)) * 0.8);
  flame =
      mix(flame, smoke_tip, smoothstep(0.82, 1.0, height_t) * (1.0 - core_mask) * 0.35);

  float ember_band =
      smoothstep(0.02, 0.22, height_t) * (1.0 - smoothstep(0.18, 0.62, height_t));
  float ember_noise = fbm(
      vec2(centered_x * 18.0 + flame_phase * 1.7, tex_coord.y * 25.0 - u_time * 6.5));
  float ember_sparks = pow(max(ember_noise - 0.72, 0.0) * 3.6, 3.0) * ember_band;
  flame += vec3(2.45, 1.2, 0.24) * ember_sparks * intensity_scale;

  float blue_core = core_mask * (1.0 - smoothstep(0.0, 0.12, height_t));
  flame += vec3(0.08, 0.18, 0.42) * blue_core * 0.3;

  float glow = pow(1.0 - height_t, 2.2) * (0.35 + 0.65 * body_mask) * u_glow_strength;
  flame += vec3(1.32, 0.66, 0.18) * glow * intensity_scale;

  flame *= flicker * mix(0.94, 1.08, tex_detail);
  flame = clamp(flame, 0.0, 4.0);
  frag_color = vec4(flame, clamp(alpha, 0.0, 1.0));
}
