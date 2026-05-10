#version 330 core

flat in vec3 v_instanceColor;
flat in float v_instanceAlpha;
in float v_distFromCenter;

uniform float u_time;

out vec4 FragColor;

void main() {
  vec3 color = v_instanceColor;

  float edgeGlow = smoothstep(0.3, 0.8, v_distFromCenter);
  vec3 glowColor = v_instanceColor * 1.5;
  color = mix(color, glowColor, edgeGlow * 0.4);

  float pulse = sin(u_time * 2.5) * 0.5 + 0.5;
  color += v_instanceColor * pulse * 0.2;

  float ripple = sin(v_distFromCenter * 10.0 - u_time * 4.0) * 0.5 + 0.5;
  color += v_instanceColor * ripple * 0.15 * edgeGlow;

  float edgeFade = 1.0 - smoothstep(0.7, 1.0, v_distFromCenter);
  float alpha = v_instanceAlpha * edgeFade;

  alpha += edgeGlow * 0.3;

  FragColor = vec4(color, clamp(alpha, 0.0, 1.0));
}
