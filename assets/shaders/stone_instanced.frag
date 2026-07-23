#version 330 core

in vec3 v_world_pos;
in vec3 v_normal;
in vec3 v_color;
in vec3 v_local_pos;

uniform vec3 u_light_direction;

out vec4 frag_color;

float hash31(vec3 p) {
  p = fract(p * 0.1031);
  p += dot(p, p.yzx + 33.33);
  return fract((p.x + p.y) * p.z);
}

float noise3(vec3 p) {
  vec3 i = floor(p);
  vec3 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  return mix(
      mix(mix(hash31(i), hash31(i + vec3(1.0, 0.0, 0.0)), f.x),
          mix(hash31(i + vec3(0.0, 1.0, 0.0)), hash31(i + vec3(1.0, 1.0, 0.0)), f.x),
          f.y),
      mix(mix(hash31(i + vec3(0.0, 0.0, 1.0)), hash31(i + vec3(1.0, 0.0, 1.0)), f.x),
          mix(hash31(i + vec3(0.0, 1.0, 1.0)), hash31(i + vec3(1.0, 1.0, 1.0)), f.x),
          f.y),
      f.z);
}

void main() {
  vec3 N = normalize(v_normal);
  vec3 L = normalize(u_light_direction);
  vec3 V = normalize(vec3(0.0, 0.86, 0.52));
  vec3 H = normalize(L + V);

  vec3 p = v_local_pos;
  float broad = noise3(p * 3.2 + vec3(1.7, 5.1, 2.3));
  float grain = noise3(p * 13.0 + v_world_pos * 0.12);
  float strata = 0.5 + 0.5 * sin(p.y * 22.0 + p.x * 4.0 + broad * 3.2);
  float fissure_field = abs(sin(p.x * 13.0 - p.z * 9.0 + p.y * 6.0 + broad * 4.0));
  float fissures = 1.0 - smoothstep(0.035, 0.14, fissure_field);

  vec3 stone = v_color * mix(0.70, 1.08, broad);
  stone *= mix(0.86, 1.08, grain * 0.65 + strata * 0.35);
  stone = mix(stone, stone * vec3(0.55, 0.58, 0.60), fissures * 0.72);

  float upward = smoothstep(0.28, 0.86, N.y);
  float lichen_noise = noise3(v_world_pos * 1.8 + vec3(3.0, 8.0, 1.0));
  float lichen = upward * smoothstep(0.58, 0.82, lichen_noise) * 0.34;
  vec3 lichen_color = vec3(0.24, 0.28, 0.20);
  stone = mix(stone, lichen_color, lichen);

  float ground_damp = (1.0 - smoothstep(0.02, 0.24, p.y)) *
                      smoothstep(0.28, 0.72, noise3(v_world_pos * 0.9));
  stone = mix(stone, stone * vec3(0.52, 0.58, 0.60), ground_damp * 0.58);

  float ndotl = max(dot(N, L), 0.0);
  float hemi = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
  vec3 sky = vec3(0.48, 0.56, 0.66);
  vec3 sun = vec3(0.92, 0.82, 0.68);
  vec3 illumination = sky * (0.16 + hemi * 0.13) + sun * ndotl * 0.64;
  float crevice_ao = mix(1.0, 0.62, fissures) * mix(0.58, 1.0, hemi);

  float wet_spec = ground_damp * pow(max(dot(N, H), 0.0), 34.0) * 0.16;
  float dry_spec = pow(max(dot(N, H), 0.0), 18.0) * 0.025;
  float rim = pow(1.0 - max(dot(N, V), 0.0), 4.0) * 0.055;

  vec3 color = stone * illumination * crevice_ao;
  color += sun * (dry_spec + wet_spec);
  color += sky * rim;
  frag_color = vec4(color, 1.0);
}
