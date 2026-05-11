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
  vec3 light_dir = normalize(vec3(0.65, 0.50, 0.40));

  // Warm sun, cool sky ambient
  vec3 sun_color = vec3(1.08, 0.92, 0.74);
  vec3 sky_color = vec3(0.72, 0.80, 1.00);

  float wrap = 0.28;
  float ndl = dot(normal, light_dir);
  float diff_raw = ndl * (1.0 - wrap) + wrap;
  float diff = max(diff_raw, 0.15);

  // Blend toward cool sky in shadowed areas
  float lit_t = clamp((diff_raw + 1.0) / 2.5, 0.0, 1.0);
  vec3 light_tint = mix(sky_color * 0.34, sun_color, lit_t);

  vec3 color = clamp(base * diff * light_tint, 0.0, 1.0);
  FragColor = vec4(color, u_alpha);
}
