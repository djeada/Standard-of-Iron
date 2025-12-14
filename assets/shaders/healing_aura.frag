#version 330 core

in vec3 v_worldPos;
in vec3 v_normal;
in vec2 v_texCoord;
in float v_height;
in float v_radialDist;

uniform float u_time;
uniform float u_intensity;
uniform vec3 u_auraColor;

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
  return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x),
             mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x), f.y);
}

void main() {

  vec3 coreColor = vec3(1.0, 1.0, 0.7);
  vec3 midColor = u_auraColor;
  vec3 edgeColor = u_auraColor * 0.7;

  float edgeFade = smoothstep(0.2, 0.9, v_radialDist);
  float heightFade = 1.0 - smoothstep(0.0, 1.0, v_height);

  float shellFade = edgeFade * heightFade;

  vec3 color = mix(midColor, edgeColor, v_radialDist);

  float angle = atan(v_worldPos.z, v_worldPos.x);
  float swirl = sin(angle * 4.0 + u_time * 2.0 + v_height * 5.0) * 0.5 + 0.5;
  color += coreColor * swirl * 0.2 * shellFade;

  float ring = sin(v_height * 15.0 - u_time * 3.0) * 0.5 + 0.5;
  ring = pow(ring, 2.0);
  color += midColor * ring * 0.3 * edgeFade;

  vec2 particleUV = vec2(angle * 2.0, v_height * 3.0 - u_time * 1.5);
  float particles = noise(particleUV * 6.0);
  particles = pow(particles, 2.0) * 2.0;
  color += coreColor * particles * shellFade * 0.2;

  float alpha = shellFade * u_intensity * 0.7;
  alpha += edgeFade * 0.15 * u_intensity;

  FragColor = vec4(color, clamp(alpha, 0.0, 0.85));
}
