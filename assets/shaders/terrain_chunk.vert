#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;

uniform mat4 u_mvp;
uniform mat4 u_model;
uniform vec2 u_noise_offset;
uniform float u_height_noise_strength;
uniform float u_height_noise_frequency;

out vec3 v_world_pos;
out vec3 v_normal;
out vec2 v_uv;
out float v_vertex_displacement;
out float v_entry_mask;
out float v_feature_foot;

float hash21(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise21(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);

  float a = hash21(i);
  float b = hash21(i + vec2(1.0, 0.0));
  float c = hash21(i + vec2(0.0, 1.0));
  float d = hash21(i + vec2(1.0, 1.0));

  vec2 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm3(vec2 p) {
  float value = 0.0;
  float amplitude = 0.5;
  for (int i = 0; i < 3; ++i) {
    value += noise21(p) * amplitude;
    p = p * 2.13 + 11.47;
    amplitude *= 0.48;
  }
  return value;
}

mat2 rot2(float angle) {
  float c = cos(angle);
  float s = sin(angle);
  return mat2(c, -s, s, c);
}

float sample_terrain_displacement(vec3 wp,
                                  vec3 world_normal,
                                  vec2 noise_offset,
                                  float height_noise_strength,
                                  float height_noise_frequency,
                                  float entry_mask) {
  float frequency = max(height_noise_frequency, 0.0001);
  float angle =
      fract(sin(dot(noise_offset, vec2(12.9898, 78.233))) * 43758.5453) * 6.2831853;
  vec2 uv = rot2(angle) * (wp.xz + noise_offset);

  float h0 = fbm3(uv * frequency * 0.55 + vec2(19.1, -7.3)) * 2.0 - 1.0;
  float h1 = fbm3(uv * frequency) * 2.0 - 1.0;
  float h2 = fbm3(uv * frequency * 2.7) * 2.0 - 1.0;
  float h = h0 * 0.45 + h1 * 0.35 + h2 * 0.20;

  float flatness = clamp(world_normal.y, 0.0, 1.0);
  float slope = 1.0 - flatness;
  float shoulder_mask = smoothstep(0.04, 0.32, slope);
  shoulder_mask = mix(shoulder_mask, 1.0, smoothstep(0.55, 0.85, slope) * 0.35);
  float flat_ground_mask = smoothstep(0.92, 0.995, flatness);
  shoulder_mask = max(shoulder_mask, flat_ground_mask * 0.12);
  shoulder_mask *= 1.0 - 0.60 * entry_mask;

  float height_amp = clamp(height_noise_strength * 1.35, 0.0, 0.22);
  return h * height_amp * shoulder_mask;
}

void main() {
  vec3 base_wp = (u_model * vec4(a_position, 1.0)).xyz;
  vec3 world_normal = normalize(mat3(u_model) * a_normal);
  float entry_mask = clamp(a_uv.y, 0.0, 1.0);
  float feature_foot = clamp(a_uv.x, 0.0, 1.0);

  float displacement = sample_terrain_displacement(base_wp,
                                                   world_normal,
                                                   u_noise_offset,
                                                   u_height_noise_strength,
                                                   u_height_noise_frequency,
                                                   entry_mask);
  vec3 wp = base_wp;
  wp.y += displacement;

  float sample_step = 0.35;
  vec3 dx = vec3(sample_step, 0.0, 0.0);
  vec3 dz = vec3(0.0, 0.0, sample_step);
  vec3 px = base_wp + dx;
  vec3 pz = base_wp + dz;
  px.y += sample_terrain_displacement(px,
                                      world_normal,
                                      u_noise_offset,
                                      u_height_noise_strength,
                                      u_height_noise_frequency,
                                      entry_mask);
  pz.y += sample_terrain_displacement(pz,
                                      world_normal,
                                      u_noise_offset,
                                      u_height_noise_strength,
                                      u_height_noise_frequency,
                                      entry_mask);
  vec3 displaced_normal = normalize(cross(pz - wp, px - wp));
  if (dot(displaced_normal, world_normal) < 0.0) {
    displaced_normal = -displaced_normal;
  }

  v_world_pos = wp;
  v_normal = normalize(mix(world_normal, displaced_normal, 0.75));
  v_uv = a_uv;
  v_vertex_displacement = displacement;
  v_entry_mask = entry_mask;
  v_feature_foot = feature_foot;

  gl_Position = u_mvp * vec4(wp, 1.0);
}
