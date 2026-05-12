#version 330 core

in vec3 v_world_pos;
in vec3 v_normal;
in vec3 v_color;

uniform vec3 u_light_direction;

out vec4 frag_color;

void main() {
  vec3 normal = normalize(v_normal);
  vec3 light_dir = normalize(u_light_direction);

  float diffuse = max(dot(normal, light_dir), 0.0);

  float ambient = 0.22;
  float lighting = ambient + diffuse * 0.70;

  vec3 sun_color = vec3(1.08, 0.92, 0.74);
  vec3 sky_color = vec3(0.72, 0.80, 1.00);
  float lit_t = clamp(diffuse * 1.4, 0.0, 1.0);
  vec3 light_tint = mix(sky_color * 0.50, sun_color, lit_t);

  vec3 color = v_color * lighting * light_tint;

  frag_color = vec4(color, 1.0);
}
