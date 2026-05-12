#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;

uniform vec3 uLightDirection;

out vec4 FragColor;

void main() {
  vec3 normal = normalize(vNormal);
  vec3 lightDir = normalize(uLightDirection);

  float diffuse = max(dot(normal, lightDir), 0.0);

  float ambient = 0.28;
  vec3 sun_color = vec3(1.06, 0.94, 0.78);
  vec3 sky_color = vec3(0.68, 0.78, 1.00);
  float lit_t = clamp(diffuse * 1.3, 0.0, 1.0);
  vec3 light_tint = mix(sky_color * 0.45, sun_color, lit_t);

  float lighting = ambient + diffuse * 0.65;
  vec3 color = vColor * lighting * light_tint;

  FragColor = vec4(color, 1.0);
}
