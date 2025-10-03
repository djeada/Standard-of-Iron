#version 330 core

in vec2 v_uv;
in float v_soft;

uniform vec3 u_color;     // smoke color (gray)
uniform float u_alpha;    // overall alpha multiplier

out vec4 FragColor;

// Simple soft circular falloff with slight turb via v_soft
void main() {
    float r2 = dot(v_uv, v_uv); // uv in [-1,1], center at 0
    if (r2 > 1.0) discard;
    float edge = smoothstep(1.0, 0.6, r2);
    float a = edge * (0.8 + 0.2 * v_soft) * u_alpha;
    FragColor = vec4(u_color, a);
}
