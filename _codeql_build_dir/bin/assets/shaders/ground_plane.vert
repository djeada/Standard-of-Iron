#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
uniform mat4 u_mvp;
uniform mat4 u_model;
uniform vec2 u_noiseOffset;
uniform float u_heightNoiseStrength;
uniform float u_heightNoiseFrequency;
out vec3 v_worldPos;
out vec3 v_normal;
out vec2 v_uv;

float hash21(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}
float noise21(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  float a = hash21(i), b = hash21(i + vec2(1, 0)), c = hash21(i + vec2(0, 1)),
        d = hash21(i + vec2(1, 1));
  vec2 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}
float fbm2(vec2 p) {
  float v = 0.0, a = 0.5;
  for (int i = 0; i < 2; ++i) {
    v += noise21(p) * a;
    p = p * 2.07 + 13.17;
    a *= 0.5;
  }
  return v;
}
mat2 rot2(float a) {
  float c = cos(a), s = sin(a);
  return mat2(c, -s, s, c);
}

void main() {
  vec3 wp = (u_model * vec4(a_position, 1.0)).xyz;
  vec3 wn = normalize(mat3(u_model) * a_normal);
  float ang =
      fract(sin(dot(u_noiseOffset, vec2(12.9898, 78.233))) * 43758.5453) *
      6.2831853;
  vec2 uv = rot2(ang) * (wp.xz + u_noiseOffset);
  float h = fbm2(uv * u_heightNoiseFrequency) * 2.0 - 1.0;
  float amp = clamp(u_heightNoiseStrength, 0.0, 0.20);
  float disp = h * amp;
  wp.y += disp;
  v_worldPos = wp;
  v_normal = wn;
  v_uv = a_uv;
  gl_Position = u_mvp * vec4(wp, 1.0);
}
