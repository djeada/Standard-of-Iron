#version 330 core

in vec3 v_world_pos;
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

void main() {

  vec3 n = normalize(v_normal);
  vec3 l = normalize(u_light_direction);
  float diffuse = max(dot(n, l), 0.0);
  float ambient = 0.25;
  float lighting = ambient + diffuse * 0.62;

  vec3 sun_color = vec3(1.08, 0.92, 0.74);
  vec3 sky_color = vec3(0.72, 0.80, 1.00);
  float lit_t = clamp(diffuse * 1.4, 0.0, 1.0);
  vec3 light_tint = mix(sky_color * 0.50, sun_color, lit_t);

  vec2 leaf_pos = v_local_pos_xz * 120.0 + vec2(v_leaf_seed * 17.3, v_bark_seed * 23.1);

  float leaf_layer1 = hash(floor(leaf_pos));
  float leaf_layer2 = hash(floor(leaf_pos * 1.7 + vec2(5.3, 8.7)));
  float leaf_layer3 = hash(floor(leaf_pos * 0.6 + vec2(13.1, 3.9)));

  float leaf_density = (leaf_layer1 + leaf_layer2 + leaf_layer3) / 3.0;

  float leaf_edge = noise2_d(leaf_pos * 2.5);
  float leaf_fine = hash(leaf_pos * 0.37 + vec2(v_branch_id * 7.0));

  vec3 leaf_dark_green = vec3(0.22, 0.32, 0.20);
  vec3 leaf_mid_green = vec3(0.32, 0.42, 0.28);
  vec3 leaf_light_green = vec3(0.42, 0.50, 0.38);
  vec3 leaf_silver = vec3(0.52, 0.56, 0.50);

  float color_choice = leaf_fine;
  vec3 leaf_color = mix(leaf_dark_green, leaf_mid_green, color_choice);

  float backfacing = 1.0 - max(dot(n, l), 0.0);
  float silver_show = smoothstep(0.4, 0.8, backfacing) * leaf_layer2;
  leaf_color = mix(leaf_color, leaf_silver, silver_show * 0.5);

  float highlight = smoothstep(0.7, 0.9, leaf_layer1 * leaf_edge);
  leaf_color = mix(leaf_color, leaf_light_green, highlight * 0.4);

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

  float alpha = 1.0;

  if (v_foliage_mask > 0.1) {

    float leaf_mask = leaf_density;

    float hole_pattern = noise2_d(leaf_pos * 0.8);
    float holes = smoothstep(0.20, 0.35, hole_pattern);

    float cluster_gaps = noise2_d(v_local_pos_xz * 15.0 + vec2(v_leaf_seed * 3.0));
    float gaps = smoothstep(0.15, 0.40, cluster_gaps);

    float edge_dist = length(v_local_pos_xz);
    float edge_fade = 1.0 - smoothstep(0.25, 0.50, edge_dist) * 0.5;

    float top_fade = 1.0 - smoothstep(0.85, 1.0, v_tex_coord.y) * 0.4;

    alpha = leaf_mask * holes * gaps * edge_fade * top_fade;
    alpha = mix(1.0, alpha, v_foliage_mask);

    if (leaf_fine > 0.82) {
      alpha *= 0.2;
    }
  }

  alpha *= smoothstep(0.0, 0.06, v_tex_coord.y);

  if (alpha < 0.15)
    discard;

  alpha = clamp(alpha * 1.3, 0.0, 1.0);

  frag_color = vec4(color, alpha);
}
