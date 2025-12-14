#version 330 core

in vec3 v_worldPos;
in vec3 v_normal;
in vec2 v_texCoord;
in float v_beamT;
in float v_edgeDist;
in float v_glowIntensity;

uniform float u_time;
uniform float u_progress;
uniform vec3 u_healColor;
uniform float u_alpha;

out vec4 FragColor;

float hash(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);

  float a = hash(i);
  float b = hash(i + vec2(1.0, 0.0));
  float c = hash(i + vec2(0.0, 1.0));
  float d = hash(i + vec2(1.0, 1.0));

  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
  float value = 0.0;
  float amplitude = 0.5;
  for (int i = 0; i < 4; i++) {
    value += amplitude * noise(p);
    p *= 2.0;
    amplitude *= 0.5;
  }
  return value;
}

void main() {

  vec3 coreColor = vec3(0.95, 0.95, 0.6);
  vec3 innerColor = vec3(0.4, 1.0, 0.5);
  vec3 outerColor = vec3(0.2, 0.8, 0.3);
  vec3 sparkleColor = vec3(1.0, 1.0, 0.8);

  innerColor = mix(innerColor, u_healColor, 0.5);
  outerColor = mix(outerColor, u_healColor * 0.7, 0.5);

  float centerDist = v_edgeDist;

  float coreGlow = exp(-centerDist * 8.0);

  float innerGlow = exp(-centerDist * 3.0);

  float outerGlow = exp(-centerDist * 1.5);

  vec3 color = outerColor * outerGlow;
  color = mix(color, innerColor, innerGlow);
  color = mix(color, coreColor, coreGlow);

  vec2 sparkleUV = v_texCoord * 40.0 + vec2(u_time * 3.0, 0.0);
  float sparkle = noise(sparkleUV) * noise(sparkleUV * 1.5);
  sparkle = pow(sparkle, 4.0) * 2.0;
  color += sparkleColor * sparkle * innerGlow;

  vec2 flowUV = vec2(v_beamT * 10.0 - u_time * 4.0, centerDist * 5.0);
  float flow = fbm(flowUV);
  color += innerColor * flow * 0.3 * innerGlow;

  float pulse = 0.8 + 0.2 * sin(u_time * 8.0 + v_beamT * 15.0);
  color *= pulse;

  float edgeShimmer = sin(v_beamT * 50.0 - u_time * 10.0) * 0.5 + 0.5;
  edgeShimmer *= (1.0 - innerGlow) * outerGlow;
  color += sparkleColor * edgeShimmer * 0.2;

  float alpha = v_glowIntensity * outerGlow * u_alpha;

  float startFade = smoothstep(0.0, 0.1, v_beamT);
  float endFade = smoothstep(u_progress, u_progress - 0.1, v_beamT);
  alpha *= startFade * endFade;

  if (v_beamT > 0.9 && u_progress > 0.95) {
    float impactGlow = (v_beamT - 0.9) * 10.0;
    impactGlow *= sin(u_time * 15.0) * 0.5 + 0.5;
    color += coreColor * impactGlow * 0.5;
    alpha += impactGlow * 0.3;
  }

  float bloom = max(0.0, (coreGlow - 0.5) * 2.0);
  color += coreColor * bloom * 0.3;

  FragColor = vec4(color, clamp(alpha, 0.0, 1.0));
}
