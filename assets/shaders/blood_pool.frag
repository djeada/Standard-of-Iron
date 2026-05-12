#version 330 core
in vec2 v_uv;
out vec4 frag_color;

uniform float u_radius;
uniform float u_alpha_scale;

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

  float edge_noise = (noise(c * 4.5) - 0.5) * 0.35;
  float alpha = smoothstep(1.0 + edge_noise, 0.55 + edge_noise * 0.5, dist);
  alpha *= u_alpha_scale;

  vec3 color = vec3(0.28, 0.04, 0.03);
  float crust = (noise(c * 9.0) - 0.5) * 0.08;
  float dry_ring = smoothstep(0.45, 0.85, dist) * 0.35;
  color = clamp(color + vec3(crust) - vec3(dry_ring * 0.1), 0.0, 1.0);

  if (alpha < 0.01)
    discard;
  frag_color = vec4(color, alpha * 0.78);
}
