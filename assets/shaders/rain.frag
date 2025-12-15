#version 330 core

in float v_alpha;

uniform vec3 u_rain_color;

out vec4 frag_color;

void main() { frag_color = vec4(u_rain_color, v_alpha * 0.6); }
