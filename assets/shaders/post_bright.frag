#version 330 core

in vec2 v_uv;

uniform sampler2D u_scene;
uniform float u_threshold;
uniform float u_knee;

out vec4 frag_color;

const vec3 k_bright_luma = vec3(0.2126, 0.7152, 0.0722);

void main() {
  vec3 scene = texture(u_scene, v_uv).rgb;
  float luma = dot(scene, k_bright_luma);
  float soft = clamp(luma - u_threshold + u_knee, 0.0, 2.0 * u_knee);
  soft = soft * soft / max(4.0 * u_knee, 1e-4);
  float contribution = max(soft, luma - u_threshold) / max(luma, 1e-4);
  frag_color = vec4(scene * clamp(contribution, 0.0, 1.0), 1.0);
}
