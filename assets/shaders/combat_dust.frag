#version 330 core

in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_texcoord;
in float v_intensity;
in float v_alpha;

out vec4 frag_color;

uniform vec3 u_dust_color;
uniform float u_time;

void main() {

  float noise1 =
      fract(sin(dot(v_texcoord * 10.0, vec2(12.9898, 78.233))) * 43758.5453);
  float noise2 =
      fract(sin(dot(v_texcoord * 15.0 + u_time * 0.1, vec2(93.9898, 67.345))) *
            23421.631);
  float combined_noise = (noise1 + noise2) * 0.5;

  vec2 centered_uv = v_texcoord * 2.0 - 1.0;
  float dist_from_center = length(centered_uv);
  float particle_alpha = smoothstep(1.0, 0.3, dist_from_center);

  float final_alpha = v_alpha * particle_alpha * (0.5 + 0.5 * combined_noise);

  vec3 color = u_dust_color;
  color = mix(color, color * 0.8, noise1 * 0.3);

  float scatter = max(0.0, v_normal.y) * 0.2;
  color += vec3(scatter);

  frag_color = vec4(color, final_alpha * 0.6);
}
