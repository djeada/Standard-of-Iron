#version 330 core
#include "activity_indicator.glsl"

in vec3 v_normal;
in float v_layer;
in float v_radial;
in float v_phase;

uniform vec3 u_mode_color;
uniform float u_alpha;
uniform float u_time;

out vec4 frag_color;

void main() {
  frag_color = activity_indicator_shade(
      v_layer, v_radial, v_normal, u_mode_color, u_alpha, u_time, v_phase);
}
