#version 330 core

in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_texcoord;
in float v_intensity;
in float v_alpha;

out vec4 frag_color;

uniform vec3 u_dust_color;
uniform float u_time;
uniform vec3 u_center;
uniform float u_radius;
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

  vec3 color = u_dust_color;

  if (u_effect_type == 0) {

    color = u_dust_color;
    color = mix(color, color * 0.8, noise1 * 0.3);

    float scatter = max(0.0, v_normal.y) * 0.2;
    color += vec3(scatter);

    frag_color = vec4(color, final_alpha * 0.6);
  } else if (u_effect_type == 1) {

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
  } else if (u_effect_type == 2) {

    float height = v_texcoord.y;
    float angle_t = v_texcoord.x;
    float t = u_time * 0.5;

    float chunk_count = 24.0;
    float chunk_id = floor(angle_t * chunk_count);
    float chunk_local = fract(angle_t * chunk_count);
    float chunk_hash = fract(sin(chunk_id * 127.1 + 311.7) * 43758.5453);

    float noise1 =
        fract(sin(dot(v_texcoord * 20.0 + t * 0.3, vec2(12.9898, 78.233))) *
              43758.5453);
    float noise2 = fract(
        sin(dot(v_texcoord * 45.0 + chunk_id, vec2(93.989, 67.345))) * 23421.6);
    float combined_noise = mix(noise1, noise2, 0.5);

    vec3 dust_dark = vec3(0.35, 0.30, 0.22);
    vec3 dust_mid = vec3(0.55, 0.48, 0.38);
    vec3 dust_light = vec3(0.75, 0.68, 0.55);
    vec3 rock_dark = vec3(0.25, 0.22, 0.18);
    vec3 rock_mid = vec3(0.40, 0.36, 0.30);

    float is_rock = step(0.6, chunk_hash) * step(height, 0.5);

    if (is_rock > 0.5) {
      color = mix(rock_dark, rock_mid, combined_noise);
      color *= 0.85 + 0.15 * sin(t * 5.0 + chunk_id);
    } else {
      if (height < 0.25) {
        color = mix(dust_dark, dust_mid, height / 0.25);
      } else if (height < 0.6) {
        color = mix(dust_mid, dust_light, (height - 0.25) / 0.35);
      } else {
        color = mix(dust_light, vec3(0.9, 0.85, 0.75), (height - 0.6) / 0.4);
      }

      float billow = 0.9 + 0.1 * sin(t * 2.0 + chunk_id * 1.5 + height * 4.0);
      color *= billow;
    }

    color *= 0.9 + 0.2 * combined_noise;

    float phase = smoothstep(0.0, 0.15, t);
    float decay = 1.0 - smoothstep(2.5, 5.0, t);

    float core_glow = (1.0 - height) * (1.0 - smoothstep(0.0, 0.4, t)) * 0.5;
    color += vec3(1.0, 0.8, 0.4) * core_glow;

    color *= v_intensity * 1.1;

    float chunk_fade =
        smoothstep(0.0, 0.15, chunk_local) * smoothstep(1.0, 0.85, chunk_local);
    float density = 0.5 + 0.5 * (1.0 - height);

    float radial = length(v_world_pos.xz - u_center.xz) / max(u_radius, 0.01);
    float radial_fade = 1.0 - smoothstep(0.8, 2.0, radial);

    float impact_alpha = v_alpha * chunk_fade * density * radial_fade *
                         (0.8 + 0.2 * combined_noise);

    impact_alpha = clamp(impact_alpha * 1.3, 0.0, 0.95);

    color = clamp(color, 0.0, 1.5);
    frag_color = vec4(color, impact_alpha);
  } else {

    color = u_dust_color;
    frag_color = vec4(color, final_alpha * 0.6);
  }
}
