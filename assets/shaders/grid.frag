#version 330 core

in vec3 v_normal;
in vec2 v_tex_coord;
in vec3 v_world_pos;

uniform vec3 u_grid_color;
uniform vec3 u_line_color;
uniform float u_cell_size;
uniform float u_thickness;

out vec4 frag_color;

float hash12(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

void main() {
  vec2 coord = v_world_pos.xz / u_cell_size;
  vec2 f = fract(coord) - 0.5;
  vec2 af = abs(f);

  float fw = fwidth(af.x);
  float line_x =
      1.0 - smoothstep(0.5 - u_thickness - fw, 0.5 - u_thickness + fw, af.x);
  fw = fwidth(af.y);
  float line_y =
      1.0 - smoothstep(0.5 - u_thickness - fw, 0.5 - u_thickness + fw, af.y);
  float line_mask = max(line_x, line_y);

  vec2 cell = floor(coord);
  float major =
      (abs(mod(cell.x, 5.0)) < 0.5 || abs(mod(cell.y, 5.0)) < 0.5) ? 1.0 : 0.0;
  vec3 line_col = mix(u_line_color, u_line_color * 1.2, major);

  float jitter = (hash12(cell) - 0.5) * 0.06;
  vec3 base = u_grid_color * (1.0 + jitter);

  vec3 col = mix(base, line_col, line_mask);
  frag_color = vec4(col, 1.0);
}
