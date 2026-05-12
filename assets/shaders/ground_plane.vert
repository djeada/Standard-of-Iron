#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
uniform mat4 u_mvp;
uniform mat4 u_model;
uniform vec2 u_noise_offset;
uniform float u_noise_angle;
uniform float u_tile_size;
uniform float u_height_noise_strength;
uniform float u_height_noise_frequency;
out vec3 v_world_pos;
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
  for (int i = 0; i < 3; ++i) {
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
  vec3 world_normal = normalize(mat3(u_model) * a_normal);
  float ts = max(u_tile_size, 1e-4);
  vec2 uv = rot2(u_noise_angle) * ((wp.xz / ts) + u_noise_offset);

  float warp_base = fbm2(uv * 0.35);
  float warp_off = fbm2((uv + vec2(11.3, -7.7)) * 0.35);
  vec2 uv_warp = uv + (vec2(warp_base, warp_off) - 0.5) * 0.9;

  float freq = max(u_height_noise_frequency * 1.85, 0.75);
  float broad_a = fbm2(uv_warp * max(freq * 0.105, 0.045));
  float broad_b = fbm2((uv_warp + vec2(-19.4, 27.1)) * max(freq * 0.19, 0.070));
  float broad_ridge = 1.0 - abs(broad_a * 2.0 - 1.0);
  float broad = (broad_a * 0.58 + broad_b * 0.27 + broad_ridge * 0.15) * 2.0 - 1.0;
  float base = fbm2(uv_warp * freq * 0.55);
  float detail = fbm2(uv_warp * freq * 1.35);
  float fine = noise21(uv_warp * freq * 3.4);
  float directional = sin(uv_warp.x * 1.45 + detail * 1.1) * 0.10 +
                      sin(uv_warp.y * 1.70 - base * 0.9) * 0.08;
  float h = (base * 0.40 + detail * 0.34 + fine * 0.18 + broad_a * 0.08 +
             directional) *
                2.0 -
            1.0;

  h = h - 0.16 * h * h * h;

  float strength = clamp(u_height_noise_strength, 0.02, 1.0);
  float amp = clamp(0.055 + strength * 0.22, 0.055, 0.30);
  float broad_amp = clamp(0.10 + strength * 0.46, 0.10, 0.56);
  float disp = h * amp + broad * broad_amp;

  disp += directional * amp * 0.85;

  float grad_step = max(0.08, 0.22 / max(freq, 0.05));
  float broad_a_x =
      fbm2((uv_warp + vec2(grad_step, 0.0)) * max(freq * 0.105, 0.045));
  float broad_b_x = fbm2(((uv_warp + vec2(grad_step, 0.0)) + vec2(-19.4, 27.1)) *
                         max(freq * 0.19, 0.070));
  float broad_x = (broad_a_x * 0.58 + broad_b_x * 0.27 +
                   (1.0 - abs(broad_a_x * 2.0 - 1.0)) * 0.15) *
                      2.0 -
                  1.0;
  float h_base_x = fbm2((uv_warp + vec2(grad_step, 0.0)) * freq * 0.55);
  float h_det_x = fbm2((uv_warp + vec2(grad_step, 0.0)) * freq * 1.35);
  float h_fin_x = noise21((uv_warp + vec2(grad_step, 0.0)) * freq * 3.4);
  float h_dir_x = sin((uv_warp.x + grad_step) * 1.45 + h_det_x * 1.1) * 0.10 +
                  sin(uv_warp.y * 1.70 - h_base_x * 0.9) * 0.08;
  float hx = (h_base_x * 0.40 + h_det_x * 0.34 + h_fin_x * 0.18 +
              broad_a_x * 0.08 + h_dir_x) *
                 2.0 -
             1.0;

  float broad_a_z =
      fbm2((uv_warp + vec2(0.0, grad_step)) * max(freq * 0.105, 0.045));
  float broad_b_z = fbm2(((uv_warp + vec2(0.0, grad_step)) + vec2(-19.4, 27.1)) *
                         max(freq * 0.19, 0.070));
  float broad_z = (broad_a_z * 0.58 + broad_b_z * 0.27 +
                   (1.0 - abs(broad_a_z * 2.0 - 1.0)) * 0.15) *
                      2.0 -
                  1.0;
  float h_base_z = fbm2((uv_warp + vec2(0.0, grad_step)) * freq * 0.55);
  float h_det_z = fbm2((uv_warp + vec2(0.0, grad_step)) * freq * 1.35);
  float h_fin_z = noise21((uv_warp + vec2(0.0, grad_step)) * freq * 3.4);
  float h_dir_z = sin(uv_warp.x * 1.45 + h_det_z * 1.1) * 0.10 +
                  sin((uv_warp.y + grad_step) * 1.70 - h_base_z * 0.9) * 0.08;
  float hz = (h_base_z * 0.40 + h_det_z * 0.34 + h_fin_z * 0.18 +
              broad_a_z * 0.08 + h_dir_z) *
                 2.0 -
             1.0;

  float normal_amp = amp * 2.8;
  vec3 dx = vec3(grad_step * ts,
                 (hx - h) * normal_amp + (broad_x - broad) * broad_amp, 0.0);
  vec3 dz = vec3(0.0, (hz - h) * normal_amp + (broad_z - broad) * broad_amp,
                 grad_step * ts);
  vec3 warped_normal = normalize(cross(dz, dx));
  vec3 wn = normalize(mix(world_normal, warped_normal, 0.72));
  vec3 displaced = wp + vec3(0.0, disp, 0.0);

  v_world_pos = displaced;
  v_normal = wn;
  v_uv = a_uv;
  v_disp = disp;
  gl_Position = u_mvp * vec4(displaced, 1.0);
}
