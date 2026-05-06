#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
uniform mat4 u_mvp;
uniform mat4 u_model;
uniform vec2 u_noiseOffset;
uniform float u_noiseAngle;
uniform float u_tileSize;
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
  float ts = max(u_tileSize, 1e-4);
  vec2 uv = rot2(u_noiseAngle) * ((wp.xz / ts) + u_noiseOffset);

  float warpBase = fbm2(uv * 0.35);
  float warpOff = fbm2((uv + vec2(11.3, -7.7)) * 0.35);
  vec2 uvWarp = uv + (vec2(warpBase, warpOff) - 0.5) * 0.9;

  float freq = max(u_heightNoiseFrequency * 1.85, 0.75);
  float base = fbm2(uvWarp * freq * 0.55);
  float detail = fbm2(uvWarp * freq * 1.35);
  float fine = noise21(uvWarp * freq * 3.4);
  float directional = sin(uvWarp.x * 1.45 + detail * 1.1) * 0.10 +
                      sin(uvWarp.y * 1.70 - base * 0.9) * 0.08;
  float h =
      (base * 0.46 + detail * 0.34 + fine * 0.20 + directional) * 2.0 - 1.0;

  h = h - 0.16 * h * h * h;

  float strength = clamp(u_heightNoiseStrength, 0.02, 1.0);
  float amp = clamp(0.03 + strength * 0.11, 0.03, 0.11);
  float disp = h * amp;

  disp += directional * amp * 0.65;

  float gradStep = max(0.08, 0.22 / max(freq, 0.05));
  float h_base_x = fbm2((uvWarp + vec2(gradStep, 0.0)) * freq * 0.55);
  float h_det_x = fbm2((uvWarp + vec2(gradStep, 0.0)) * freq * 1.35);
  float h_fin_x = noise21((uvWarp + vec2(gradStep, 0.0)) * freq * 3.4);
  float h_dir_x = sin((uvWarp.x + gradStep) * 1.45 + h_det_x * 1.1) * 0.10 +
                  sin(uvWarp.y * 1.70 - h_base_x * 0.9) * 0.08;
  float hx =
      (h_base_x * 0.46 + h_det_x * 0.34 + h_fin_x * 0.20 + h_dir_x) * 2.0 - 1.0;

  float h_base_z = fbm2((uvWarp + vec2(0.0, gradStep)) * freq * 0.55);
  float h_det_z = fbm2((uvWarp + vec2(0.0, gradStep)) * freq * 1.35);
  float h_fin_z = noise21((uvWarp + vec2(0.0, gradStep)) * freq * 3.4);
  float h_dir_z = sin(uvWarp.x * 1.45 + h_det_z * 1.1) * 0.10 +
                  sin((uvWarp.y + gradStep) * 1.70 - h_base_z * 0.9) * 0.08;
  float hz =
      (h_base_z * 0.46 + h_det_z * 0.34 + h_fin_z * 0.20 + h_dir_z) * 2.0 - 1.0;

  float normalAmp = amp * 2.4;
  vec3 dx = vec3(gradStep * ts, (hx - h) * normalAmp, 0.0);
  vec3 dz = vec3(0.0, (hz - h) * normalAmp, gradStep * ts);
  vec3 warpedNormal = normalize(cross(dz, dx));
  vec3 wn = normalize(mix(worldNormal, warpedNormal, 0.72));
  vec3 displaced = wp + vec3(0.0, disp, 0.0);

  v_worldPos = displaced;
  v_normal = wn;
  v_uv = a_uv;
  v_disp = disp;
  gl_Position = u_mvp * vec4(displaced, 1.0);
}
