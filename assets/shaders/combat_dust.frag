#version 330 core

in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_texcoord;
in float v_intensity;
in float v_alpha;

out vec4 frag_color;

uniform vec3 u_dust_color;
uniform float u_time;
uniform int u_effect_type; // 0 = Dust, 1 = Flame

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
    // Dust effect
    color = u_dust_color;
    color = mix(color, color * 0.8, noise1 * 0.3);

    float scatter = max(0.0, v_normal.y) * 0.2;
    color += vec3(scatter);

    frag_color = vec4(color, final_alpha * 0.6);
  } else {
    // Flame effect
    float flame_height = v_texcoord.y;
    
    // Create flame color gradient from yellow-orange at bottom to red-orange at top
    vec3 base_color = mix(vec3(1.2, 0.6, 0.15), vec3(1.0, 0.3, 0.08), flame_height);
    vec3 core_glow = vec3(1.5, 0.95, 0.45);
    
    // Add bright core in the lower part of the flame
    float core_factor = pow(1.0 - flame_height, 2.5) * 0.7;
    color = mix(base_color, core_glow, core_factor);
    
    // Add flicker using noise
    float flicker = mix(0.85, 1.3, combined_noise);
    color *= flicker;
    
    // Modulate by intensity
    color *= v_intensity;
    
    // Add glow at the base
    float glow = pow(1.0 - flame_height, 3.0) * 1.2;
    color += vec3(1.3, 0.65, 0.2) * glow * v_intensity;
    
    // Fade at edges horizontally
    float edge_fade = smoothstep(0.0, 0.2, v_texcoord.x) * 
                      smoothstep(0.0, 0.2, 1.0 - v_texcoord.x);
    
    // Height-based fade
    float height_fade = smoothstep(1.0, 0.4, flame_height);
    
    float flame_alpha = edge_fade * height_fade * final_alpha * 0.8;
    
    color = clamp(color, 0.0, 3.5);
    frag_color = vec4(color, clamp(flame_alpha, 0.0, 1.0));
  }
}
