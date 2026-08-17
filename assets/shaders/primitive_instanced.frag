#version 330 core
#include "directional_shadows.glsl"
#include "environment_lighting.glsl"
#include "local_lighting.glsl"

in vec3 v_world_pos;
in vec3 v_normal;
in vec3 v_color;
in float v_alpha;

out vec4 frag_color;

void main() {
  vec3 normal = normalize(v_normal);
  vec3 color = v_color * environment_lighting(normal, 0.0);
  color += v_color * local_lighting(v_world_pos, normal);
  color = apply_directional_shadow(color, v_world_pos, normal);
  frag_color = vec4(color, v_alpha);
}
