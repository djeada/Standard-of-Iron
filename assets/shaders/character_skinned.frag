#version 330 core

in vec3 v_normal_ws;
in vec2 v_tex;
in vec3 v_pos_ws;
flat in int v_color_role;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_useTexture;
uniform float u_alpha;
uniform int u_materialId;
uniform vec3 u_role_colors[32];
uniform int u_role_color_count;

out vec4 FragColor;

void main() {
  vec3 base = u_color;
  if (v_color_role > 0 && v_color_role <= u_role_color_count) {
    base = u_role_colors[v_color_role - 1];
  }
  if (u_useTexture) {
    base *= texture(u_texture, v_tex).rgb;
  }

  vec3 normal = normalize(v_normal_ws);
  vec3 light_dir = normalize(vec3(1.0, 1.15, 1.0));
  float wrap = 0.35;
  float diff = max(dot(normal, light_dir) * (1.0 - wrap) + wrap, 0.25);

  vec3 color = clamp(base * diff, 0.0, 1.0);
  FragColor = vec4(color, u_alpha);
}
