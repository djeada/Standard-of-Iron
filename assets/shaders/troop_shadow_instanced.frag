#version 330 core
#include "contact_shadow.glsl"

in vec2 v_tex_coord;
in vec3 v_world_pos;
in float v_alpha;

uniform vec2 u_ground_light_dir;
uniform float u_cast_weight;

out vec4 frag_color;

void main() {
  frag_color = contact_shadow_color(
      v_tex_coord, v_world_pos, u_ground_light_dir, u_cast_weight, v_alpha);
}
