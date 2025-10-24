#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;

uniform vec3 uLightDirection;

out vec4 FragColor;

void main() {
  vec3 normal = normalize(vNormal);
  vec3 lightDir = normalize(uLightDirection);

  // Diffuse lighting
  float diffuse = max(dot(normal, lightDir), 0.0);

  // Ambient + diffuse
  float ambient = 0.4;
  float lighting = ambient + diffuse * 0.6;

  vec3 color = vColor * lighting;

  FragColor = vec4(color, 1.0);
}
