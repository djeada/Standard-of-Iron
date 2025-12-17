#version 330 core

in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_texcoord;
in float v_intensity;
in float v_alpha;

out vec4 frag_color;

uniform vec3 u_dust_color;
uniform float u_time;
uniform int u_effect_type;

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

  vec3 color;

  if (u_effect_type == 0) {

    color = u_dust_color;
    color = mix(color, color * 0.8, noise1 * 0.3);

    float scatter = max(0.0, v_normal.y) * 0.2;
    color += vec3(scatter);

    frag_color = vec4(color, final_alpha * 0.6);
  } else {
    // Flame effect - realistic fire colors with strong visibility
    float flame_height = v_texcoord.y;

    // More vibrant fire colors - hot white/yellow at base, orange in middle, red at tips
    vec3 base_color = vec3(1.8, 1.4, 0.6);  // Hot yellow-white base
    vec3 mid_color = vec3(1.6, 0.7, 0.2);   // Bright orange middle
    vec3 tip_color = vec3(1.2, 0.35, 0.1);  // Red-orange tips

    // Create gradient from base to tip
    color = mix(base_color, mid_color, smoothstep(0.0, 0.5, flame_height));
    color = mix(color, tip_color, smoothstep(0.5, 1.0, flame_height));

    // Add intense core glow for realism
    float core_glow_factor = pow(1.0 - flame_height, 2.0) * smoothstep(0.3, 0.0, dist_from_center);
    color += vec3(2.0, 1.5, 0.8) * core_glow_factor;

    // Strong flicker using noise for dynamic fire
    float flicker = mix(0.9, 1.4, combined_noise);
    color *= flicker;

    // Apply intensity
    color *= v_intensity;

    // Additional bright glow at the base
    float base_glow = pow(1.0 - flame_height, 3.0) * 1.8;
    color += vec3(1.8, 1.0, 0.3) * base_glow * v_intensity;

    // Less aggressive edge fade - keep flames visible
    float edge_fade = smoothstep(0.0, 0.15, v_texcoord.x) *
                      smoothstep(0.0, 0.15, 1.0 - v_texcoord.x);

    // Keep more of the flame visible at height
    float height_fade = smoothstep(1.0, 0.3, flame_height);

    // Much stronger alpha - multiply by 1.8 instead of 0.8
    float flame_alpha = edge_fade * height_fade * final_alpha * 1.8;

    color = clamp(color, 0.0, 5.0);
    frag_color = vec4(color, clamp(flame_alpha, 0.0, 1.0));
  }
}
