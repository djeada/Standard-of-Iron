#version 330 core

in vec3 v_world_pos;
in vec3 v_normal;
in vec3 v_color;
in vec3 v_local_pos;

uniform vec3 u_light_direction;

out vec4 frag_color;

float hash12(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise21(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  return mix(mix(hash12(i), hash12(i + vec2(1.0, 0.0)), f.x),
             mix(hash12(i + vec2(0.0, 1.0)), hash12(i + vec2(1.0)), f.x), f.y);
}

void main() {
  vec3 N = normalize(v_normal);
  vec3 L = normalize(u_light_direction);
  vec3 V = normalize(vec3(0.0, 0.86, 0.52));
  vec3 H = normalize(L + V);

  float coarse = noise21(v_world_pos.xz * 1.35 + v_local_pos.xy * 2.1);
  float pits = noise21(v_world_pos.xz * 6.5 + v_local_pos.zy * 5.0);
  float bedding = 0.5 + 0.5 * sin(v_local_pos.y * 17.0 + coarse * 4.2);
  vec3 stone = v_color * mix(0.67, 1.06, coarse);
  stone *= mix(0.82, 1.05, bedding * 0.55 + pits * 0.45);

  float upward = smoothstep(0.32, 0.88, N.y);
  float lichen_field = noise21(v_world_pos.xz * 2.7 + vec2(v_local_pos.y * 1.3));
  float lichen = upward * smoothstep(0.54, 0.82, lichen_field) * 0.42;
  stone = mix(stone, vec3(0.22, 0.27, 0.20), lichen);

  float vertical_face = 1.0 - smoothstep(0.35, 0.82, abs(N.y));
  float rain_path = noise21(vec2(v_world_pos.x * 2.2 + v_world_pos.z,
                                  floor(v_local_pos.y * 3.0) * 0.37));
  float rain_stain = vertical_face * smoothstep(0.60, 0.88, rain_path) *
                     (1.0 - smoothstep(0.35, 1.65, v_local_pos.y));
  stone = mix(stone, stone * vec3(0.48, 0.54, 0.53), rain_stain * 0.60);

  float fracture_field = abs(sin(v_local_pos.x * 15.0 - v_local_pos.z * 11.0 +
                                  v_local_pos.y * 8.0 + coarse * 3.0));
  float fracture = 1.0 - smoothstep(0.025, 0.11, fracture_field);
  stone *= mix(1.0, 0.58, fracture * 0.55);

  float base_stain = 1.0 - smoothstep(0.02, 0.38, v_local_pos.y);
  stone = mix(stone, stone * vec3(0.48, 0.54, 0.50), base_stain * 0.50);

  float ndotl = max(dot(N, L), 0.0);
  float hemi = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
  vec3 sky = vec3(0.46, 0.54, 0.65);
  vec3 sun = vec3(0.92, 0.82, 0.68);
  vec3 illumination = sky * (0.15 + hemi * 0.14) + sun * ndotl * 0.62;
  float ao = mix(0.48, 1.0, hemi) * mix(1.0, 0.68, fracture);
  float specular = pow(max(dot(N, H), 0.0), 28.0) * (0.025 + rain_stain * 0.09);
  float rim = pow(1.0 - max(dot(N, V), 0.0), 4.0) * 0.045;

  vec3 color = stone * illumination * ao;
  color += sun * specular;
  color += sky * rim;
  frag_color = vec4(color, 1.0);
}
