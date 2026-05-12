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

  float ambient = 0.28;
  vec3 sun_color = vec3(1.06, 0.94, 0.78);
  vec3 sky_color = vec3(0.68, 0.78, 1.00);
  float lit_t = clamp(diffuse * 1.3, 0.0, 1.0);
  vec3 light_tint = mix(sky_color * 0.45, sun_color, lit_t);

  float lighting = ambient + diffuse * 0.65;
  vec3 color = v_color * lighting * light_tint;

  frag_color = vec4(color, 1.0);
}
