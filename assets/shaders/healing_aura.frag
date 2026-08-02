#version 330 core
#include "noise.glsl"

in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_tex_coord;
in float v_height;
in float v_radial_dist;

uniform float u_time;
uniform float u_intensity;
uniform vec3 u_aura_color;

out vec4 frag_color;

float noise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  return mix(mix(soi_hash_82bbee(i), soi_hash_82bbee(i + vec2(1.0, 0.0)), f.x),
             mix(soi_hash_82bbee(i + vec2(0.0, 1.0)),
                 soi_hash_82bbee(i + vec2(1.0, 1.0)),
                 f.x),
             f.y);
}

void main() {

  vec3 core_color = vec3(1.0, 1.0, 0.74);
  vec3 mid_color = u_aura_color;
  vec3 edge_color = u_aura_color * 0.72;

  float perimeter_mask = smoothstep(0.56, 0.94, v_radial_dist);
  float crown_fade = 1.0 - smoothstep(0.58, 1.0, v_height);
  float base_fade = 1.0 - smoothstep(0.84, 1.0, v_height);

  float veil = perimeter_mask * crown_fade;

  vec3 color = mix(mid_color, edge_color, v_radial_dist);

  float angle = atan(v_world_pos.z, v_world_pos.x);
  float swirl = sin(angle * 4.0 + u_time * 2.0 + v_height * 5.0) * 0.5 + 0.5;
  color += core_color * swirl * 0.08 * veil;

  float ring = sin(v_height * 15.0 - u_time * 3.0) * 0.5 + 0.5;
  ring = pow(ring, 2.0);
  color += mid_color * ring * 0.12 * perimeter_mask;

  vec2 particle_uv = vec2(angle * 2.0, v_height * 3.0 - u_time * 1.5);
  float particles = noise(particle_uv * 6.0);
  particles = pow(particles, 2.0) * 2.0;
  color += core_color * particles * veil * 0.07;

  float filament = pow(swirl, 3.0) * mix(0.35, 1.0, ring);
  float aura_alpha = veil * (0.018 + filament * 0.082) * u_intensity;
  aura_alpha += perimeter_mask * base_fade * ring * u_intensity * 0.014;

  frag_color = vec4(color, clamp(aura_alpha, 0.0, 0.12));
}
