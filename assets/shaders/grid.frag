#version 330 core

in vec3 v_normal;
in vec2 v_texCoord;
in vec3 v_worldPos;

uniform vec3 u_gridColor;
uniform vec3 u_lineColor;
uniform float u_cellSize;
uniform float u_thickness; // fraction of cell (0..0.5)

out vec4 FragColor;

void main() {
    vec2 coord = v_worldPos.xz / u_cellSize;
    vec2 g = abs(fract(coord) - 0.5);
    float lineX = step(0.5 - u_thickness, g.x);
    float lineY = step(0.5 - u_thickness, g.y);
    float lineMask = max(lineX, lineY);
    vec3 col = mix(u_gridColor, u_lineColor, lineMask);
    FragColor = vec4(col, 1.0);
}
