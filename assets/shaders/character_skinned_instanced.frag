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

out vec4 frag_color;

void main() {
  vec3 base = v_color;
  if (v_color_role > 0) {
    base = texelFetch(u_role_color_tbo, v_instance_id * 32 + v_color_role - 1).rgb;
  }

  vec3 normal = normalize(v_normal_ws);
  vec3 light_dir = normalize(vec3(0.65, 0.50, 0.40));

  vec3 sun_color = vec3(1.08, 0.92, 0.74);
  vec3 sky_color = vec3(0.72, 0.80, 1.00);

  float wrap = 0.28;
  float ndl = dot(normal, light_dir);
  float diff_raw = ndl * (1.0 - wrap) + wrap;
  float diff = max(diff_raw, 0.15);

  float lit_t = clamp((diff_raw + 1.0) / 2.5, 0.0, 1.0);
  vec3 light_tint = mix(sky_color * 0.34, sun_color, lit_t);

  vec3 color = clamp(base * diff * light_tint, 0.0, 1.0);
  frag_color = vec4(color, v_alpha);
}
