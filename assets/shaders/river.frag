#version 330 core
out vec4 frag_color;
in vec2 tex_coord;
in vec3 world_pos;

uniform float time;
uniform sampler2D u_visibility_tex;
uniform vec2 u_visibility_size;
uniform float u_visibility_tile_size;
uniform float u_explored_alpha;
uniform int u_has_visibility;
uniform float u_segment_visibility;
uniform int u_water_surface_kind;

const float PI = 3.14159265359;
float saturate(float x) {
  return clamp(x, 0.0, 1.0);
}
vec3 saturate(vec3 v) {
  return clamp(v, vec3(0.0), vec3(1.0));
}
mat2 rot(float a) {
  float c = cos(a), s = sin(a);
  return mat2(c, -s, s, c);
}

float hash(vec2 p) {
  p = fract(p * vec2(123.34, 456.21));
  p += dot(p, p + 45.32);
  return fract(p.x * p.y);
}
float noise(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float a = hash(i), b = hash(i + vec2(1, 0)), c = hash(i + vec2(0, 1)),
        d = hash(i + vec2(1, 1));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
float fbm(vec2 p) {
  float v = 0., a = .5, f = 1.;
  for (int i = 0; i < 5; i++) {
    v += a * noise(p * f);
    f *= 2.;
    a *= .5;
  }
  return v;
}

vec3 sky_color(vec3 rd, vec3 sun_dir) {
  float t = saturate(rd.y * 0.5 + 0.5);
  vec3 horizon = vec3(0.68, 0.84, 0.95), zenith = vec3(0.15, 0.36, 0.70);
  vec3 sky = mix(horizon, zenith, t);
  float sun = pow(max(dot(rd, sun_dir), 0.0), 260.0);
  float halo = pow(max(dot(rd, sun_dir), 0.0), 6.0) * 0.03;
  return sky + vec3(1.0, 0.96, 0.88) * (sun * 1.0 + halo);
}
float fresnel_schlick(float c, float F0) {
  return F0 + (1.0 - F0) * pow(1.0 - c, 5.0);
}
float ggx_spec(vec3 N, vec3 V, vec3 L, float rough, float F0) {
  vec3 H = normalize(V + L);
  float NdotV = max(dot(N, V), 0.0), NdotL = max(dot(N, L), 0.0);
  float NdotH = max(dot(N, H), 0.0), VdotH = max(dot(V, H), 0.0);
  float a = max(rough * rough, 0.001), a2 = a * a;
  float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
  float D = a2 / (PI * denom * denom);
  float k = (a + 1.0);
  k = (k * k) / 8.0;
  float Gv = NdotV / (NdotV * (1.0 - k) + k), Gl = NdotL / (NdotL * (1.0 - k) + k);
  float F = fresnel_schlick(VdotH, F0);
  return (D * Gv * Gl * F) / max(4.0 * NdotV * NdotL, 0.001);
}

float water_height(vec2 uv) {
  vec2 p = uv;
  vec2 w1 = vec2(fbm(p * 0.6 + time * 0.05),
                 fbm(p * 0.6 - time * 0.04));
  vec2 w2 = vec2(fbm(rot(1.3) * p * 0.9 - time * 0.03),
                 fbm(rot(2.1) * p * 0.7 + time * 0.02));
  p += 0.75 * w1 + 0.45 * w2;
  float h = 0.0;
  h += 0.55 * (fbm(p * 1.6 - time * 0.15) - 0.5);
  h += 0.30 * (fbm(rot(0.8) * p * 2.8 + time * 0.20) - 0.5);
  h += 0.15 * (fbm(rot(2.4) * p * 5.0 - time * 0.35) - 0.5);
  return h;
}

void height_deriv(vec2 uv, out float h, out vec2 grad, out float lap) {
  float s = max(0.003, 0.35 * length(fwidth(uv)));
  vec2 e = vec2(s);
  float hc = water_height(uv);
  float hx1 = water_height(uv + vec2(e.x, 0));
  float hx2 = water_height(uv - vec2(e.x, 0));
  float hy1 = water_height(uv + vec2(0, e.y));
  float hy2 = water_height(uv - vec2(0, e.y));
  grad = vec2((hx1 - hx2) / (2.0 * e.x), (hy1 - hy2) / (2.0 * e.y)) * 0.85;
  lap = (hx1 + hx2 + hy1 + hy2 - 4.0 * hc) / (e.x * e.x + e.y * e.y);
  h = hc;
}

vec3 micro_normal(vec2 uv) {
  float s = 7.0;
  vec2 e = vec2(max(0.0015, 0.5 * length(fwidth(uv))));
  float mx1 = fbm(uv * s + time * 0.6 + vec2(e.x, 0)),
        mx2 = fbm(uv * s + time * 0.6 - vec2(e.x, 0));
  float my1 = fbm(uv * s + time * 0.6 + vec2(0, e.y)),
        my2 = fbm(uv * s + time * 0.6 - vec2(0, e.y));
  vec2 g = vec2((mx1 - mx2) / (2.0 * e.x), (my1 - my2) / (2.0 * e.y));
  return normalize(vec3(-g.x, 0.35, -g.y));
}
vec3 water_normal(vec2 uv, vec2 grad) {
  float k = 3.2;
  vec3 N = normalize(vec3(-grad.x * k, 1.0, -grad.y * k));
  return normalize(
      mix(N,
          normalize(N + 0.22 * (micro_normal(uv) - vec3(0, 1, 0))),
          0.35));
}

void main() {

  vec2 uv = rot(0.35) * (world_pos.xz * 0.38);

  float h, lap;
  vec2 grad;
  height_deriv(uv, h, grad, lap);
  vec3 N = water_normal(uv, grad);

  vec3 sun_dir = normalize(vec3(0.28, 0.85, 0.43));
  vec3 V = normalize(vec3(0.0, 0.7, 0.7));

  float NdotV = max(dot(N, V), 0.0);
  float F0 = 0.02;

  // Rivers and lakes are one hydrological system. Sampling the same material
  // in world space makes a tributary-to-lake join visually continuous.
  vec3 deep_water = vec3(0.010, 0.044, 0.054);
  vec3 shallow_water = vec3(0.034, 0.122, 0.128);

  float calm = smoothstep(0.0, 0.45, abs(h));
  float shallow = saturate(0.35 + 0.35 * (fbm(uv * 0.6) * (1.0 - calm)));

  vec3 absorb = vec3(0.90, 0.45, 0.12);
  float thickness = mix(0.6, 3.5, 1.0 - shallow) * (0.35 + pow(1.0 - NdotV, 0.7));
  vec3 trans_base = mix(deep_water, shallow_water, shallow);
  vec3 transmission = trans_base * exp(-absorb * thickness);

  vec3 R = reflect(-V, N);
  vec3 reflection = sky_color(R, sun_dir);
  reflection *= 0.32;
  reflection *= vec3(0.48, 0.62, 0.72);
  float F = fresnel_schlick(NdotV, F0) * 0.24;

  float NdotL = max(dot(N, sun_dir), 0.0);
  float rough = mix(0.12,
                    0.26,
                    smoothstep(0.0, 0.6, length(grad)));
  float spec = ggx_spec(N, V, sun_dir, rough, F0) * 0.50;
  vec3 spec_col = vec3(0.62, 0.70, 0.78) * spec;
  vec3 sun_diffuse = transmission * NdotL * 0.20;

  float shore_distance = u_water_surface_kind == 1
                             ? tex_coord.y
                             : min(tex_coord.x, 1.0 - tex_coord.x);
  float shore = 1.0 - smoothstep(0.035, 0.20, shore_distance);
  float foam = shore * (0.45 + 0.55 * fbm(uv * 3.0 + time * 0.6));
  vec3 foam_col = vec3(0.92, 0.96, 1.0);
  foam = clamp(foam * 0.24, 0.0, 1.0);

  vec3 color = transmission * (1.0 - F) + reflection * F;
  color += spec_col + sun_diffuse;
  color = mix(color, foam_col * mix(0.82, 1.0, NdotL), foam);

  color += vec3(0.012, 0.030, 0.040) * pow(1.0 - NdotV, 3.0);

  float visibility_factor = 1.0;
  if (u_has_visibility == 1 && u_visibility_size.x > 0.0 && u_visibility_size.y > 0.0) {
    float tile_size = max(u_visibility_tile_size, 0.0001);
    vec2 grid = vec2(world_pos.x / tile_size, world_pos.z / tile_size);
    grid += (u_visibility_size * 0.5) - vec2(0.5);
    vec2 vis_uv = (grid + vec2(0.5)) / u_visibility_size;
    float vis_sample = texture(u_visibility_tex, vis_uv).r;
    if (vis_sample < 0.25) {
      discard;
    } else if (vis_sample < 0.75) {
      visibility_factor = u_explored_alpha;
    }
  }
  visibility_factor *= u_segment_visibility;
  color *= visibility_factor;

  // Water meshes overlap at authored bends and tributary junctions. Keep the
  // surface opaque so those clean architectural joints do not turn into dark
  // alpha seams.
  frag_color = vec4(saturate(color), 1.0);
}
