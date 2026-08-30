#version 330 core
#include "directional_shadows.glsl"
#include "environment_lighting.glsl"
#include "local_lighting.glsl"
#include "noise.glsl"
#include "visibility_mask.glsl"

in vec3 v_world_pos;
in vec3 v_normal;
in vec3 v_color;
in vec3 v_local_pos;
flat in float v_seed;

uniform vec3 u_camera_pos;
uniform float u_time;
uniform float u_magic_strength;

out vec4 frag_color;

float fbm(vec3 p) {
  float v = 0.0;
  float a = 0.5;
  for (int i = 0; i < 4; i++) {
    v += soi_noise3(p) * a;
    p = p * 2.03 + vec3(17.13, 7.91, 11.47);
    a *= 0.5;
  }
  return v;
}

void main() {
  vec3 N = normalize(v_normal);
  vec3 L = environment_primary_direction();
  vec3 V = normalize(u_camera_pos - v_world_pos);
  vec3 H = normalize(L + V);

  vec3 p = v_local_pos * 1.15;

  float rock_large = fbm(p * 1.5 + vec3(2.0, 5.0, 1.0));
  float rock_grain = fbm(p * 8.2 + vec3(1.0, 9.0, 3.0));
  float rust = fbm(p * 3.1 + vec3(v_seed * 5.0, 0.0, 2.0));

  vec3 slate = v_color * vec3(0.70, 0.74, 0.84);
  vec3 umber = v_color * vec3(1.05, 0.82, 0.58);
  vec3 soot = vec3(0.09, 0.08, 0.08);
  vec3 rock_color = mix(slate, umber, rust * 0.7);
  rock_color *= mix(0.70, 1.12, rock_grain);
  rock_color = mix(rock_color, soot, rock_large * 0.22);

  float side_face = 1.0 - abs(N.y);
  float crack_a = 1.0 - smoothstep(0.0, 0.05, abs(fract(v_local_pos.y * 3.4) - 0.5));
  float crack_b =
      1.0 - smoothstep(0.0,
                       0.05,
                       abs(fract((v_local_pos.x + v_local_pos.z) * 2.1 + 0.3) - 0.5));
  float cracks = max(crack_a, crack_b) * side_face;
  rock_color *= 1.0 - cracks * 0.22;

  float seam_field = fbm(p * 2.6 + vec3(v_seed * 3.0, 1.0, 7.0));
  float seam = 1.0 - smoothstep(0.0, 0.03, abs(seam_field - 0.5));
  float fine_vein =
      1.0 - smoothstep(0.0, 0.02, abs(fbm(p * 6.0 + vec3(4.0, v_seed, 2.0)) - 0.5));
  float ore = max(seam, fine_vein * 0.5) * smoothstep(0.02, 0.20, v_local_pos.y);

  float crystal = smoothstep(0.62, 0.72, v_local_pos.y);
  float outlying = smoothstep(0.26, 0.32, v_local_pos.y) *
                   smoothstep(0.66, 0.72, length(v_local_pos.xz));
  crystal = max(crystal, outlying);

  vec3 gold_deep = vec3(0.62, 0.40, 0.08);
  vec3 gold_bright = vec3(1.08, 0.82, 0.30);
  float facet = fbm(p * 4.0 + vec3(9.0, v_seed * 2.0, 5.0));
  vec3 gold = mix(gold_deep, gold_bright, facet);

  vec3 base_color = mix(rock_color, gold, max(crystal, ore * 0.45));

  vec3 sun_color = environment_primary_color() * environment_primary_intensity();
  vec3 sky_color = environment_sky_color();

  float ao = clamp(N.y * 0.45 + 0.70, 0.26, 1.0);
  vec3 ambient = environment_ambient_light(N);
  vec3 direct = soi_key_light(N) * 0.80;

  float metal = max(crystal, ore);
  float spec_power = mix(22.0, 96.0, metal);
  float spec_gain = mix(0.08, 0.55, metal);
  float specular = pow(max(dot(N, H), 0.0), spec_power) * spec_gain;
  float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);

  vec3 color = base_color * (ambient + direct) * ao * environment_exposure();
  color += soi_rim_light(N, V) * mix(0.6, 1.4, metal);
  color += sun_color * specular * ao * mix(vec3(1.0), gold_bright, metal);
  color += sky_color * fresnel * 0.08;

  float strength = max(u_magic_strength, 0.0);
  float pulse = 0.55 + 0.45 * sin(u_time * 1.6 + v_seed * 6.28318 +
                                  fbm(p * 1.3 + vec3(u_time * 0.22)) * 5.2);
  vec3 curse = vec3(0.92, 0.14, 0.06);
  vec3 ember = vec3(1.10, 0.62, 0.16);
  float core = (1.0 - smoothstep(0.10, 0.36, length(v_local_pos.xz))) *
               smoothstep(0.50, 0.80, v_local_pos.y);
  color += mix(ember, curse, pulse) * strength *
           (ore * (0.30 + 0.30 * pulse) + core * 0.30 * pulse);

  color += gold * crystal * (0.22 + 0.06 * pulse);
  color += gold_bright * strength * crystal * (0.10 + 0.10 * pulse) * fresnel;

  color = apply_directional_shadow(color, v_world_pos, v_normal);
  color += base_color * ao * local_lighting(v_world_pos, normalize(v_normal));
  color = apply_visibility_world_shading(color, v_world_pos.xz);
  frag_color = vec4(color, 1.0);
}
