#version 330 core

in vec3 v_color;
in float v_alpha;

out vec4 fragColor;

void main() {
  if (v_alpha <= 0.02)
    discard;
  fragColor = vec4(v_color, v_alpha);
}
