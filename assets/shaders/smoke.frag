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
    // Opaque in center, fade out toward edge
    float edge = 1.0 - smoothstep(0.7, 1.0, r2);
    float a = edge * (0.85 + 0.15 * v_soft) * u_alpha;
    FragColor = vec4(u_color, a);
}
