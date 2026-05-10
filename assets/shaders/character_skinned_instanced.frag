#version 330 core

in vec3 v_normal_ws;
in vec2 v_tex;
in vec3 v_pos_ws;
flat in vec3 v_color;
flat in float v_alpha;
flat in int v_material_id;
flat in int v_color_role;
flat in int v_instance_id;

uniform samplerBuffer u_role_color_tbo;

out vec4 FragColor;

void main() {
  vec3 base = v_color;
  if (v_color_role > 0) {
    base =
        texelFetch(u_role_color_tbo, v_instance_id * 32 + v_color_role - 1).rgb;
  }

  vec3 normal = normalize(v_normal_ws);
  vec3 light_dir = normalize(vec3(1.0, 1.15, 1.0));
  float wrap = 0.35;
  float diff = max(dot(normal, light_dir) * (1.0 - wrap) + wrap, 0.25);

  vec3 color = clamp(base * diff, 0.0, 1.0);
  FragColor = vec4(color, v_alpha);
}
