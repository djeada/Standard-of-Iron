#version 330 core

in vec2 v_uv;

uniform sampler2D u_source;
uniform vec2 u_texel_step;

out vec4 frag_color;

const float k_weights[5] = float[5](0.227027, 0.194595, 0.121622, 0.054054, 0.016216);

void main() {
  vec3 accumulated = texture(u_source, v_uv).rgb * k_weights[0];
  for (int tap = 1; tap < 5; ++tap) {
    vec2 offset = u_texel_step * float(tap);
    accumulated += texture(u_source, v_uv + offset).rgb * k_weights[tap];
    accumulated += texture(u_source, v_uv - offset).rgb * k_weights[tap];
  }
  frag_color = vec4(accumulated, 1.0);
}
