#version 330 core
in vec2 v_uv;
out vec4 frag_color;

uniform float u_radius;
uniform float u_alpha_scale;
uniform float u_seed;

float hash(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}
float noise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x),
             mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}

void main() {
  vec2 c = v_uv * 2.0 - 1.0;
  float dist = length(c);

  vec2 seeded = c + vec2(u_seed * 8.31, u_seed * 3.77);
  float edge_noise =
      (noise(seeded * 4.4) - 0.5) * 0.34 + (noise(seeded * 9.5) - 0.5) * 0.16;
  float alpha = smoothstep(1.0 + edge_noise, 0.46 + edge_noise * 0.35, dist);

  float droplet_1 =
      1.0 - smoothstep(0.045, 0.12, length(c - vec2(0.47, -0.28)));
  float droplet_2 =
      1.0 - smoothstep(0.035, 0.09, length(c - vec2(-0.56, 0.18)));
  float droplet_3 = 1.0 - smoothstep(0.025, 0.07, length(c - vec2(0.18, 0.61)));
  float droplets = max(max(droplet_1, droplet_2), droplet_3);
  droplets *= smoothstep(0.45, 1.25, u_radius);
  alpha = max(alpha, droplets * 0.72);
  alpha *= u_alpha_scale;

  vec3 wet = vec3(0.22, 0.012, 0.010);
  vec3 dry = vec3(0.095, 0.018, 0.014);
  float grain = noise(seeded * 13.0) * 0.16 + noise(seeded * 27.0) * 0.05;
  float dry_ring = smoothstep(0.52, 0.98, dist);
  vec3 color = mix(wet, dry, dry_ring * 0.75);
  color += vec3(grain * 0.45, grain * 0.10, grain * 0.08);
  color *= 1.0 - smoothstep(0.72, 1.0, dist) * 0.22;

  if (alpha < 0.01)
    discard;
  frag_color = vec4(clamp(color, 0.0, 1.0), alpha * 0.86);
}
