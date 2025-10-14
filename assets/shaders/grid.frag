#version 330 core

in vec3 v_normal;
in vec2 v_texCoord;
in vec3 v_worldPos;

uniform vec3 u_gridColor;
uniform vec3 u_lineColor;
uniform float u_cellSize;
uniform float u_thickness; // fraction of cell (0..0.5)

out vec4 FragColor;

// Hash for subtle per-cell variation
float hash12(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

void main() {
  vec2 coord = v_worldPos.xz / u_cellSize;
  vec2 f = fract(coord) - 0.5;
  vec2 af = abs(f);

  // Anti-aliased lines using fwidth
  float fw = fwidth(af.x);
  float lineX =
      1.0 - smoothstep(0.5 - u_thickness - fw, 0.5 - u_thickness + fw, af.x);
  fw = fwidth(af.y);
  float lineY =
      1.0 - smoothstep(0.5 - u_thickness - fw, 0.5 - u_thickness + fw, af.y);
  float lineMask = max(lineX, lineY);

  // Emphasize major lines every 5 cells
  vec2 cell = floor(coord);
  float major =
      (abs(mod(cell.x, 5.0)) < 0.5 || abs(mod(cell.y, 5.0)) < 0.5) ? 1.0 : 0.0;
  vec3 lineCol = mix(u_lineColor, u_lineColor * 1.2, major);

  // Subtle per-cell brightness jitter
  float jitter = (hash12(cell) - 0.5) * 0.06;
  vec3 base = u_gridColor * (1.0 + jitter);

  vec3 col = mix(base, lineCol, lineMask);
  FragColor = vec4(col, 1.0);
}
