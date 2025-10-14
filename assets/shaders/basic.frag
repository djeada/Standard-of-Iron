#version 330 core

in vec3 v_normal;
in vec2 v_texCoord;
in vec3 v_worldPos;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_useTexture;
uniform float u_alpha;

out vec4 FragColor;

void main() {
  vec3 color = u_color;
  if (u_useTexture) {
    color *= texture(u_texture, v_texCoord).rgb;
  }
  vec3 normal = normalize(v_normal);
  vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
  float diff = max(dot(normal, lightDir), 0.2);
  color *= diff;
  FragColor = vec4(color, u_alpha);
}