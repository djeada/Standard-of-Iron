#version 330 core

in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in float v_alpha;

uniform vec3 u_lightDir;
uniform float u_ambientStrength;

out vec4 FragColor;

void main() {
  vec3 normal = normalize(v_normal);
  vec3 lightDir = normalize(u_lightDir);

  // Diffuse lighting
  float diff = max(dot(normal, lightDir), 0.0);

  // Combine ambient and diffuse
  float lighting = u_ambientStrength + (1.0 - u_ambientStrength) * diff;

  vec3 color = v_color * lighting;
  FragColor = vec4(color, v_alpha);
}
