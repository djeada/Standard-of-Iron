#version 330 core
#include "noise.glsl"

in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_tex_coord;
in float v_beam_t;
in float v_edge_dist;
in float v_glow_intensity;

uniform float u_time;
uniform float u_progress;
uniform vec3 u_heal_color;
uniform float u_alpha;

out vec4 frag_color;

float fbm(vec2 p) {
  float value = 0.0;
  float amplitude = 0.5;
  for (int i = 0; i < 4; i++) {
    value += amplitude * soi_noise_3d41e6(p);
    p *= 2.0;
    amplitude *= 0.5;
  }
  return value;
}

void main() {

  vec3 core_color = vec3(0.95, 0.95, 0.6);
  vec3 inner_color = vec3(0.4, 1.0, 0.5);
  vec3 outer_color = vec3(0.2, 0.8, 0.3);
  vec3 sparkle_color = vec3(1.0, 1.0, 0.8);

  inner_color = mix(inner_color, u_heal_color, 0.5);
  outer_color = mix(outer_color, u_heal_color * 0.7, 0.5);

  float center_dist = v_edge_dist;

  float core_glow = exp(-center_dist * 8.0);

  float inner_glow = exp(-center_dist * 3.0);

  float outer_glow = exp(-center_dist * 1.5);

  vec3 color = outer_color * outer_glow;
  color = mix(color, inner_color, inner_glow);
  color = mix(color, core_color, core_glow);

  vec2 sparkle_uv = v_tex_coord * 40.0 + vec2(u_time * 3.0, 0.0);
  float sparkle = soi_noise_3d41e6(sparkle_uv) * soi_noise_3d41e6(sparkle_uv * 1.5);
  sparkle = pow(sparkle, 4.0) * 2.0;
  color += sparkle_color * sparkle * inner_glow;

  vec2 flow_uv = vec2(v_beam_t * 10.0 - u_time * 4.0, center_dist * 5.0);
  float flow = fbm(flow_uv);
  color += inner_color * flow * 0.3 * inner_glow;

  float pulse = 0.8 + 0.2 * sin(u_time * 8.0 + v_beam_t * 15.0);
  color *= pulse;

  float edge_shimmer = sin(v_beam_t * 50.0 - u_time * 10.0) * 0.5 + 0.5;
  edge_shimmer *= (1.0 - inner_glow) * outer_glow;
  color += sparkle_color * edge_shimmer * 0.2;

  float alpha = v_glow_intensity * outer_glow * u_alpha;

  float start_fade = smoothstep(0.0, 0.1, v_beam_t);
  float end_fade = smoothstep(u_progress, u_progress - 0.1, v_beam_t);
  alpha *= start_fade * end_fade;

  if (v_beam_t > 0.9 && u_progress > 0.95) {
    float impact_glow = (v_beam_t - 0.9) * 10.0;
    impact_glow *= sin(u_time * 15.0) * 0.5 + 0.5;
    color += core_color * impact_glow * 0.5;
    alpha += impact_glow * 0.3;
  }

  float bloom = max(0.0, (core_glow - 0.5) * 2.0);
  color += core_color * bloom * 0.3;

  frag_color = vec4(color, clamp(alpha, 0.0, 1.0));
}
