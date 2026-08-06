#version 330 core
#include "activity_indicator.glsl"

flat in vec3 v_instance_color;
flat in float v_instance_alpha;
in vec3 v_normal;
in float v_layer;
in float v_radial;
in float v_phase;
in float v_height;

uniform float u_time;

out vec4 frag_color;

void main() {
  frag_color = activity_indicator_shade(v_layer,
                                        v_radial,
                                        v_normal,
                                        v_instance_color,
                                        v_instance_alpha,
                                        u_time,
                                        v_phase,
                                        v_height);
}
