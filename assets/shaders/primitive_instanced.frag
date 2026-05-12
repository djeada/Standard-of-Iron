#version 330 core

in vec3 v_world_pos;
in vec3 v_normal;
in vec3 v_color;
in float v_alpha;

uniform vec3 u_light_dir;
uniform float u_ambient_strength;

out vec4 frag_color;

void main() {
  vec3 normal = normalize(v_normal);
  vec3 light_dir = normalize(u_light_dir);

  float diff = max(dot(normal, light_dir), 0.0);

  float lighting = u_ambient_strength + (1.0 - u_ambient_strength) * diff;

  vec3 color = v_color * lighting;
  frag_color = vec4(color, v_alpha);
}
