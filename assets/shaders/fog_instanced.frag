#version 330 core

in vec3 v_worldPos;
in vec3 v_normal;
in vec3 v_color;
in float v_alpha;
in vec2 v_texCoord;

out vec4 FragColor;

void main() {
  vec2 centered = abs(v_texCoord - vec2(0.5));
  float edge = max(centered.x, centered.y) * 2.0;
  float feather = 1.0 - smoothstep(0.42, 1.0, edge);
  float centerLift =
      smoothstep(1.0, 0.0, length(v_texCoord - vec2(0.5)) * 1.75);

  vec3 normal = normalize(v_normal);
  vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
  float diff = max(dot(normal, lightDir), 0.2);
  vec3 color = v_color * mix(0.82, 1.05, centerLift) * diff;
  float alpha = v_alpha * feather;
  if (alpha <= 0.004) {
    discard;
  }
  FragColor = vec4(color, alpha);
}
