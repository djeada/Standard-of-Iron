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
out float v_disp;

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
  vec3 worldNormal = normalize(mat3(u_model) * a_normal);
  float ang =
      fract(sin(dot(u_noiseOffset, vec2(12.9898, 78.233))) * 43758.5453) *
      6.2831853;
  vec2 uv = rot2(ang) * (wp.xz + u_noiseOffset);

  float warpBase = fbm2(uv * 0.35);
  float warpOff = fbm2((uv + vec2(11.3, -7.7)) * 0.35);
  vec2 uvWarp = uv + (vec2(warpBase, warpOff) - 0.5) * 0.7;

  float freq = max(u_heightNoiseFrequency * 3.0, 1.1);
  float base = fbm2(uvWarp * freq * 0.65);
  float detail = fbm2(uvWarp * freq * 1.6);
  float fine = noise21(uvWarp * freq * 3.2);
  float h = (base * 0.50 + detail * 0.35 + fine * 0.15) * 2.0 - 1.0;

  h = h - 0.2 * h * h * h;

  float strength = max(u_heightNoiseStrength, 0.35);
  float amp = clamp(strength * 1.8, 0.10, 0.65);
  float disp = h * amp;

  disp += sin(uvWarp.x * 1.8) * 0.05 + sin(uvWarp.y * 2.1) * 0.05;

  wp.y += disp;

  float gradStep = max(0.15, 0.35 / max(freq * 1.1, 0.05));
  float h_base_x = fbm2((uvWarp + vec2(gradStep, 0.0)) * freq * 0.65);
  float h_det_x = fbm2((uvWarp + vec2(gradStep, 0.0)) * freq * 1.6);
  float h_fin_x = noise21((uvWarp + vec2(gradStep, 0.0)) * freq * 3.2);
  float hx = (h_base_x * 0.50 + h_det_x * 0.35 + h_fin_x * 0.15) * 2.0 - 1.0;

  float h_base_z = fbm2((uvWarp + vec2(0.0, gradStep)) * freq * 0.65);
  float h_det_z = fbm2((uvWarp + vec2(0.0, gradStep)) * freq * 1.6);
  float h_fin_z = noise21((uvWarp + vec2(0.0, gradStep)) * freq * 3.2);
  float hz = (h_base_z * 0.50 + h_det_z * 0.35 + h_fin_z * 0.15) * 2.0 - 1.0;

  vec3 dx = vec3(gradStep, (hx - h) * amp, 0.0);
  vec3 dz = vec3(0.0, (hz - h) * amp, gradStep);
  vec3 warpedNormal = normalize(cross(dz, dx));
  vec3 wn = normalize(mix(worldNormal, warpedNormal, 0.85));

  v_worldPos = wp;
  v_normal = wn;
  v_uv = a_uv;
  v_disp = disp;
  gl_Position = u_mvp * vec4(wp, 1.0);
}
