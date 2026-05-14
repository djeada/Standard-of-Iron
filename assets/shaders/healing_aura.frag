#version 330 core

in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_tex_coord;
in float v_height;
in float v_radial_dist;

uniform float u_time;
uniform float u_intensity;
uniform vec3 u_aura_color;

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
  return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x),
             mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x),
             f.y);
}

void main() {

  vec3 core_color = vec3(1.0, 1.0, 0.7);
  vec3 mid_color = u_aura_color;
  vec3 edge_color = u_aura_color * 0.7;

  float edge_fade = smoothstep(0.2, 0.9, v_radial_dist);
  float height_fade = 1.0 - smoothstep(0.0, 1.0, v_height);

  float shell_fade = edge_fade * height_fade;

  vec3 color = mix(mid_color, edge_color, v_radial_dist);

  float angle = atan(v_world_pos.z, v_world_pos.x);
  float swirl = sin(angle * 4.0 + u_time * 2.0 + v_height * 5.0) * 0.5 + 0.5;
  color += core_color * swirl * 0.2 * shell_fade;

  float ring = sin(v_height * 15.0 - u_time * 3.0) * 0.5 + 0.5;
  ring = pow(ring, 2.0);
  color += mid_color * ring * 0.3 * edge_fade;

  vec2 particle_uv = vec2(angle * 2.0, v_height * 3.0 - u_time * 1.5);
  float particles = noise(particle_uv * 6.0);
  particles = pow(particles, 2.0) * 2.0;
  color += core_color * particles * shell_fade * 0.2;

  float alpha = shell_fade * u_intensity * 0.7;
  alpha += edge_fade * 0.15 * u_intensity;

  frag_color = vec4(color, clamp(alpha, 0.0, 0.85));
}
