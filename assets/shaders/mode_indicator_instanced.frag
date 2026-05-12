#version 330 core

flat in vec3 v_instance_color;
flat in float v_instance_alpha;
in float v_dist_from_center;

uniform float u_time;

out vec4 frag_color;

void main() {
  vec3 color = v_instance_color;

  float edge_glow = smoothstep(0.3, 0.8, v_dist_from_center);
  vec3 glow_color = v_instance_color * 1.5;
  color = mix(color, glow_color, edge_glow * 0.4);

  float pulse = sin(u_time * 2.5) * 0.5 + 0.5;
  color += v_instance_color * pulse * 0.2;

  float ripple = sin(v_dist_from_center * 10.0 - u_time * 4.0) * 0.5 + 0.5;
  color += v_instance_color * ripple * 0.15 * edge_glow;

  float edge_fade = 1.0 - smoothstep(0.7, 1.0, v_dist_from_center);
  float alpha = v_instance_alpha * edge_fade;

  alpha += edge_glow * 0.3;

  frag_color = vec4(color, clamp(alpha, 0.0, 1.0));
}
