#version 330 core

in vec3 v_world_pos;
in vec3 v_normal;
in vec3 v_color;
in float v_alpha;

out vec4 frag_color;

void main() {
  vec3 normal = normalize(v_normal);
  vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
  float diff = max(dot(normal, lightDir), 0.2);
  vec3 color = v_color * diff;
  frag_color = vec4(color, v_alpha);
}
