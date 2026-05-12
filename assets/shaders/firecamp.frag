#version 330 core
out vec4 frag_color;
in vec2 tex_coord;
in float intensity_val;
in float flame_phase;
in float flame_height;

uniform sampler2_d fire_texture;
uniform float u_time;
uniform float u_glow_strength;

void main() {
  float flame_height = clamp(flame_height, 0.0, 1.0);
  float intensity_scale = clamp(intensity_val, 0.6, 1.6);

  vec2 animated_uv =
      vec2(tex_coord.x, tex_coord.y + fract(u_time * 0.45 + flame_phase * 0.05));
  vec4 tex_color = texture(fire_texture, animated_uv);

  float noise_low =
      0.5 + 0.5 * sin(u_time * 2.3 + flame_phase * 1.9 + flame_height * 7.0);
  float noise_high =
      0.5 + 0.5 * sin(u_time * 4.8 + flame_phase * 3.6 + tex_coord.x * 10.0);
  float flicker = mix(noise_low, noise_high, clamp(flame_height * 1.2, 0.0, 1.0));

  vec3 base_color =
      mix(vec3(1.18, 0.56, 0.15), vec3(0.95, 0.28, 0.08), flame_height);
  vec3 core_glow = vec3(1.45, 0.92, 0.44);
  vec3 flame = mix(base_color, core_glow, pow(1.0 - flame_height, 2.4) * 0.6) *
               mix(0.85, 1.35, flicker) * intensity_scale *
               mix(vec3(1.0), vec3(1.55), clamp(tex_color.rgb, 0.0, 1.0));

  float edge_fade =
      smoothstep(0.0, 0.2, tex_coord.x) * smoothstep(0.0, 0.2, 1.0 - tex_coord.x);
  float height_fade = smoothstep(1.05, 0.42, tex_coord.y);
  float alpha = edge_fade * height_fade * mix(0.78, 1.05, flicker) *
                intensity_scale * tex_color.a;

  float glow = pow(1.0 - flame_height, 2.8) * u_glow_strength;
  flame += vec3(1.26, 0.64, 0.22) * glow * intensity_scale;

  flame = clamp(flame, 0.0, 3.2);
  frag_color = vec4(flame, clamp(alpha, 0.0, 1.0));
}
