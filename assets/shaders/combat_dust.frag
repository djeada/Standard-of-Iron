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

    float flame_height = v_texcoord.y;
    float angle_t = v_texcoord.x;

    float tongue_count = 8.0;
    float tongue_id = floor(angle_t * tongue_count);
    float tongue_local = fract(angle_t * tongue_count);
    float tongue_phase = tongue_id * 1.618;

    float noise_hi = fract(
        sin(dot(v_texcoord * 20.0 + u_time * 0.5, vec2(12.9898, 78.233))) *
        43758.5453);
    float noise_lo =
        fract(sin(dot(v_texcoord * 8.0 + u_time * 0.2 + tongue_phase,
                      vec2(93.9898, 67.345))) *
              23421.631);
    float flame_noise = mix(noise_lo, noise_hi, 0.5);

    vec3 core_color = vec3(1.5, 1.4, 1.0);
    vec3 inner_color = vec3(1.4, 0.9, 0.3);
    vec3 mid_color = vec3(1.3, 0.5, 0.15);
    vec3 outer_color = vec3(1.0, 0.25, 0.1);
    vec3 tip_color = vec3(0.4, 0.15, 0.1);

    if (flame_height < 0.15) {
      color = mix(core_color, inner_color, flame_height / 0.15);
    } else if (flame_height < 0.35) {
      color = mix(inner_color, mid_color, (flame_height - 0.15) / 0.2);
    } else if (flame_height < 0.65) {
      color = mix(mid_color, outer_color, (flame_height - 0.35) / 0.3);
    } else {
      color = mix(outer_color, tip_color, (flame_height - 0.65) / 0.35);
    }

    float hue_shift = sin(tongue_phase * 2.0) * 0.1;
    color.r += hue_shift;
    color.g -= hue_shift * 0.5;

    float flicker = 0.8 + 0.2 * sin(u_time * 10.0 + tongue_phase * 4.0 +
                                    flame_height * 8.0);
    color *= flicker;

    color *= (0.85 + 0.3 * flame_noise);

    color *= v_intensity * 1.2;

    float glow = pow(1.0 - flame_height, 2.5) * 0.8;
    color += vec3(1.2, 0.8, 0.4) * glow * v_intensity;

    float tongue_fade = smoothstep(0.0, 0.15, tongue_local) *
                        smoothstep(1.0, 0.85, tongue_local);

    float flame_alpha = v_alpha * tongue_fade * (0.7 + 0.3 * flame_noise);

    color = clamp(color, 0.0, 4.0);
    frag_color = vec4(color, clamp(flame_alpha, 0.0, 1.0));
  }
}
