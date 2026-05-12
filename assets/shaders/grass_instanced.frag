#version 330 core

in vec3 v_color;
in float v_alpha;

out vec4 frag_color;

void main() {
  if (v_alpha <= 0.02)
    discard;
  frag_color = vec4(v_color, v_alpha);
}
