#version 330 core

in vec3 v_color;
in float v_alpha;
in float v_edge;
in float v_edge_softness;

out vec4 frag_color;

void main() {

  float edge = 1.0 - smoothstep(1.0 - v_edge_softness, 1.0, abs(v_edge));
  float alpha = v_alpha * edge;
  if (alpha <= 0.02)
    discard;
  frag_color = vec4(v_color, alpha);
}
