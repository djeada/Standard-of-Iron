#version 330 core

in vec3 v_worldPos;
in vec3 v_normal;
in vec2 v_texCoord;
in float v_distFromCenter;

uniform vec3 u_modeColor;
uniform float u_alpha;
uniform float u_time;

out vec4 FragColor;

void main() {
  // Base color with mode-specific tint
  vec3 color = u_modeColor;
  
  // Add illumination effect at edges
  float edgeGlow = smoothstep(0.3, 0.8, v_distFromCenter);
  vec3 glowColor = u_modeColor * 1.5;
  color = mix(color, glowColor, edgeGlow * 0.4);
  
  // Pulsing illumination
  float pulse = sin(u_time * 2.5) * 0.5 + 0.5;
  color += u_modeColor * pulse * 0.2;
  
  // Energy ripple effect
  float ripple = sin(v_distFromCenter * 10.0 - u_time * 4.0) * 0.5 + 0.5;
  color += u_modeColor * ripple * 0.15 * edgeGlow;
  
  // Fade out at edges for smooth blending
  float edgeFade = 1.0 - smoothstep(0.7, 1.0, v_distFromCenter);
  float alpha = u_alpha * edgeFade;
  
  // Add extra glow for visibility
  alpha += edgeGlow * 0.3;
  
  FragColor = vec4(color, clamp(alpha, 0.0, 1.0));
}
