#version 330 core

in vec3 v_normal;
in vec3 v_color;
in vec2 v_tex_coord;
in float v_foliage_mask;
in float v_leaf_seed;
in float v_bark_seed;
in float v_branch_id;
in vec2 v_local_pos_xz;

uniform vec3 u_light_direction;

out vec4 frag_color;

const float PI = 3.14159265359;
const float TWO_PI = 6.28318530718;

float hash(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float hash3(vec3 p) {
  return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

float noise2_d(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float a = hash(i);
  float b = hash(i + vec2(1.0, 0.0));
  float c = hash(i + vec2(0.0, 1.0));
  float d = hash(i + vec2(1.0, 1.0));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

vec2 rotate2_d(vec2 p, float angle) {
  float s = sin(angle);
  float c = cos(angle);
  return mat2(c, -s, s, c) * p;
}

float olive_leaf_mask(vec2 p, float seed) {
  vec2 cell = floor(p);
  vec2 local = fract(p) - vec2(0.5);
  float angle = hash(cell + vec2(seed, seed * 1.7)) * TWO_PI;
  vec2 q = rotate2_d(local, angle);

  float half_length = mix(0.34, 0.46, hash(cell + vec2(3.1, 5.7) + vec2(seed)));
  float half_width = mix(0.08, 0.13, hash(cell + vec2(8.2, 1.4) + vec2(seed * 0.7)));

  q.x /= half_length;
  q.y /= half_width;

  float body = 1.0 - smoothstep(0.82, 1.08, length(q));
  float taper = 1.0 - smoothstep(0.90, 1.15, abs(q.x) + abs(q.y) * 0.30);
  float center = 1.0 - smoothstep(0.12, 0.45, abs(q.y));
  return body * taper * mix(0.90, 1.0, center);
}

void main() {

  vec3 n = normalize(v_normal);
  vec3 l = normalize(u_light_direction);
  float diffuse = max(dot(n, l), 0.0);
  float wrap = clamp((dot(n, l) + 0.28) / 1.28, 0.0, 1.0);
  float backlight = max(dot(-n, l), 0.0);
  float ambient = mix(0.26, 0.33, v_foliage_mask);
  float sky_fill = smoothstep(-0.15, 0.85, n.y) * mix(0.05, 0.12, v_foliage_mask);
  float lighting = ambient + wrap * mix(0.30, 0.48, v_foliage_mask) + sky_fill;

  vec3 sun_color = vec3(1.08, 0.92, 0.74);
  vec3 sky_color = vec3(0.72, 0.80, 1.00);
  float lit_t = clamp(wrap * 1.2, 0.0, 1.0);
  vec3 light_tint = mix(sky_color * 0.55, sun_color, lit_t);

  vec2 leaf_pos = vec2(dot(v_local_pos_xz, vec2(0.82, 0.57)), v_tex_coord.y) * 16.0 +
                  vec2(v_leaf_seed * 13.7, v_branch_id * 2.3);
  float leaf_mottle_a = noise2_d(leaf_pos);
  float leaf_mottle_b = noise2_d(leaf_pos * 2.1 + vec2(v_bark_seed * 5.0));
  float leaf_fine = hash(floor(leaf_pos * 3.0 + vec2(v_branch_id * 7.0)));
  float canopy_edge = smoothstep(0.10, 0.30, length(v_local_pos_xz));

  vec3 leaf_dark_green = vec3(0.22, 0.32, 0.20);
  vec3 leaf_mid_green = vec3(0.32, 0.42, 0.28);
  vec3 leaf_light_green = vec3(0.42, 0.50, 0.38);
  vec3 leaf_silver = vec3(0.52, 0.56, 0.50);

  float color_choice = mix(leaf_mottle_a, leaf_fine, 0.25);
  vec3 leaf_color = mix(leaf_dark_green, leaf_mid_green, color_choice);

  float backfacing = 1.0 - max(dot(n, l), 0.0);
  float silver_show = smoothstep(0.35, 0.85, backfacing) * leaf_mottle_b;
  leaf_color = mix(leaf_color, leaf_silver, silver_show * 0.35);

  float highlight = smoothstep(0.68, 0.92, leaf_mottle_a * leaf_fine);
  leaf_color = mix(leaf_color, leaf_light_green, highlight * 0.28);
  leaf_color = mix(leaf_color, leaf_light_green, canopy_edge * 0.12);

  leaf_color = mix(leaf_color, v_color, 0.15);

  float canopy_depth = 1.0 - smoothstep(0.0, 0.35, length(v_local_pos_xz));
  leaf_color *= mix(0.75, 1.0, canopy_depth);

  float bark_u = v_tex_coord.x * TWO_PI;
  float bark_v = v_tex_coord.y;

  float furrows = pow(abs(sin(bark_u * 5.0 + v_bark_seed * TWO_PI)), 0.4);
  float vertical_grain =
      noise2_d(vec2(bark_u * 3.0, bark_v * 25.0 + v_bark_seed * 7.0));
  float bark_noise = noise2_d(vec2(bark_u * 8.0, bark_v * 15.0)) * 0.3;
  float bark_texture = furrows * 0.5 + vertical_grain * 0.35 + bark_noise;

  vec3 bark_dark = vec3(0.18, 0.16, 0.13);
  vec3 bark_mid = vec3(0.30, 0.27, 0.23);
  vec3 bark_light = vec3(0.42, 0.38, 0.33);

  vec3 bark_color = mix(bark_dark, bark_mid, bark_texture);
  float bark_highlight =
      smoothstep(0.75, 0.95, hash(vec2(bark_v * 15.0, bark_u * 3.0)));
  bark_color = mix(bark_color, bark_light, bark_highlight * 0.35);

  vec3 base_color = mix(bark_color, leaf_color, v_foliage_mask);
  vec3 color = base_color * lighting * light_tint;
  color += leaf_color * vec3(0.16, 0.20, 0.09) *
           (backlight * backlight * 0.10 * v_foliage_mask);

  if (v_tex_coord.y < 0.035)
    discard;

  frag_color = vec4(color, 1.0);
}
