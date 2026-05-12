#version 330 core

in vec3 v_world_pos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_tex_coord;
in float v_foliage_mask;
in float v_needle_seed;
in float v_bark_seed;

uniform vec3 u_light_direction;

out vec4 frag_color;

const float PI = 3.14159265359;
const float TWO_PI = 6.28318530718;

float hash(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {

  vec3 n = normalize(v_normal);
  vec3 l = normalize(u_light_direction);
  float diffuse = max(dot(n, l), 0.0);
  float ambient = 0.22;
  float lighting = ambient + diffuse * 0.72;

  vec3 sun_color = vec3(1.08, 0.92, 0.74);
  vec3 sky_color = vec3(0.72, 0.80, 1.00);
  float lit_t = clamp(diffuse * 1.4, 0.0, 1.0);
  vec3 light_tint = mix(sky_color * 0.50, sun_color, lit_t);

  float needleNoise = hash(vec2(v_tex_coord.x * 28.0 + v_needle_seed * 7.1,
                                v_tex_coord.y * 24.0 + v_needle_seed * 5.3));

  float needleStreak = hash(vec2(v_tex_coord.x * 12.0 + v_needle_seed * 3.7,
                                 floor(v_tex_coord.y * 6.0 + v_needle_seed * 2.0)));

  vec3 needleColor = v_color * (0.78 + needleNoise * 0.28);
  needleColor += vec3(0.02, 0.05, 0.02) * needleStreak;

  float tipBlend = smoothstep(0.82, 1.02, v_tex_coord.y);
  needleColor =
      mix(needleColor, needleColor * vec3(1.08, 1.04, 1.10), tipBlend);

  float barkStripe = sin(v_tex_coord.y * 45.0 + v_bark_seed * TWO_PI) * 0.1 + 0.9;
  float barkNoise = hash(vec2(v_tex_coord.x * 18.0 + v_bark_seed * 4.3,
                              v_tex_coord.y * 10.0 + v_bark_seed * 7.7));

  vec3 trunkBase = vec3(0.32, 0.24, 0.16) * barkStripe;
  vec3 trunkColor = trunkBase * (0.85 + barkNoise * 0.35);

  vec3 baseColor = mix(trunkColor, needleColor, v_foliage_mask);
  vec3 color = baseColor * lighting * light_tint;

  float silhouetteNoise = hash(vec2(v_tex_coord.x * 30.0 + v_needle_seed * 9.0,
                                    v_tex_coord.y * 40.0 + v_needle_seed * 5.5));

  float alphaFoliage = 0.70 + silhouetteNoise * 0.25;
  float alpha = mix(1.0, alphaFoliage, v_foliage_mask);

  alpha *= smoothstep(0.00, 0.05, v_tex_coord.y);

  alpha = clamp(alpha, 0.0, 1.0);
  if (alpha < 0.06)
    discard;

  frag_color = vec4(color, alpha);
}
