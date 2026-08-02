#version 330 core
#include "directional_shadows.glsl"
#include "environment_lighting.glsl"
#include "local_lighting.glsl"
#include "visibility_mask.glsl"

in vec3 v_world_pos;
in vec3 v_normal;
in vec3 v_color;
in vec3 v_local_pos;

out vec4 frag_color;

float hash12(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise21(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  return mix(mix(hash12(i), hash12(i + vec2(1.0, 0.0)), f.x),
             mix(hash12(i + vec2(0.0, 1.0)), hash12(i + vec2(1.0)), f.x),
             f.y);
}

void main() {
  vec3 N = normalize(v_normal);
  vec3 L = environment_primary_direction();
  vec3 V = normalize(vec3(0.0, 0.86, 0.52));
  vec3 H = normalize(L + V);

  float grain = noise21(v_world_pos.xz * 2.1 + v_local_pos.yy * 3.4);
  float veining = noise21(v_local_pos.xz * 7.5 + vec2(v_local_pos.y * 4.5));
  vec3 marble = v_color * mix(0.90, 1.06, grain);
  marble =
      mix(marble, marble * vec3(0.86, 0.87, 0.90), smoothstep(0.62, 0.86, veining));

  float upward = smoothstep(0.45, 0.92, N.y);
  float patina_field = noise21(v_world_pos.xz * 3.1 + vec2(v_local_pos.y * 1.7));
  float patina = upward * smoothstep(0.66, 0.90, patina_field) * 0.20;
  marble = mix(marble, vec3(0.52, 0.58, 0.50), patina);

  float crevice =
      1.0 -
      smoothstep(
          0.0, 0.16, abs(sin(v_local_pos.y * 9.0 + v_local_pos.x * 6.0 + grain * 2.4)));
  marble *= mix(1.0, 0.86, crevice * 0.35);

  float plinth_soil = 1.0 - smoothstep(0.02, 0.30, v_local_pos.y);
  marble = mix(marble, marble * vec3(0.72, 0.72, 0.68), plinth_soil * 0.45);

  float ndotl = max(dot(N, L), 0.0);
  float hemi = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
  float wrap = max(dot(N, L) * 0.5 + 0.5, 0.0);
  vec3 sky = environment_sky_color();
  vec3 sun = environment_primary_color() * environment_primary_intensity();
  vec3 illumination =
      environment_ambient_light(N) + sun * mix(ndotl, wrap, 0.35) * 0.82;
  float ao = mix(0.66, 1.0, hemi);
  float specular = pow(max(dot(N, H), 0.0), 40.0) * 0.055;
  float rim = pow(1.0 - max(dot(N, V), 0.0), 3.0) * 0.09;

  vec3 color = marble * illumination * ao * environment_exposure();
  color += sun * specular;
  color += sky * rim;
  color += color * local_lighting(v_world_pos, normalize(v_normal));
  color = apply_directional_shadow(color, v_world_pos, v_normal);
  color = apply_visibility_memory(color, v_world_pos.xz);
  frag_color = vec4(color, 1.0);
}
