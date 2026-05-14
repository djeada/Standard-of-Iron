#version 330 core

in vec3 v_normal;
in vec3 v_color;
in vec2 v_tex_coord;
in float v_height;
in float v_seed;
in float v_type;
in vec3 v_tangent;
in vec3 v_bitangent;

uniform vec3 u_light_direction;

out vec4 frag_color;

float h11(float n) {
  return fract(sin(n) * 43758.5453123);
}

float aawidth_uv(vec2 uv) {
  vec2 dx = dFdx(uv), dy = dFdy(uv);
  float w = 0.5 * (length(dx) + length(dy));
  float q = exp2(floor(log2(max(w, 1e-6)) + 0.5));
  return clamp(q, 0.0015, 0.0060);
}

float quant_step(float w) {
  return exp2(floor(log2(max(w, 1e-6)) + 0.5));
}

float bush_sdf(vec2 uv, float seed) {
  vec2 p = (uv - 0.5) * vec2(1.08, 0.96);
  float sdf = 1e9;
  for (int i = 0; i < 5; i++) {
    float fi = float(i);
    float ang = fi * 1.25663706 + seed * 3.7;
    vec2 c = vec2(cos(ang), sin(ang)) * (0.18 + h11(seed * 7.9 + fi) * 0.05);
    float r = 0.30 + h11(seed * 5.7 + fi) * 0.06;
    float d = length(p - c) - r;
    sdf = min(sdf, d);
  }
  return sdf - 0.007;
}

float rosette_sdf(vec2 uv, float seed) {
  vec2 p = uv - 0.5;
  float a = atan(p.y, p.x);
  float r = length(p);
  float petals = mix(10.0, 16.0, h11(seed * 2.7));
  float wave = 0.20 + 0.06 * sin(a * petals + seed * 5.1);
  return (r - wave) - 0.006;
}

float cactus_sdf(vec2 uv, float seed) {
  vec2 p = (uv - 0.5) * vec2(0.92, 1.08);
  float sdf = length(p) - 0.48;
  for (int i = 0; i < 3; i++) {
    float fi = float(i);
    float ang = mix(-1.6, 1.6, h11(seed * 3.3 + fi));
    vec2 c = vec2(0.22 * cos(ang), 0.12 + 0.25 * abs(sin(ang)));
    vec2 e = vec2(0.22, 0.30) * mix(0.7, 1.1, h11(seed * 6.1 + fi));
    float d = length((p - c) / e) - 1.0;
    sdf = min(sdf, d);
  }
  return sdf - 0.006;
}

float plant_sdf(vec2 uv, float type_val, float seed) {
  if (type_val < 0.45)
    return bush_sdf(uv, seed);
  if (type_val < 0.80)
    return rosette_sdf(uv, seed);
  return cactus_sdf(uv, seed);
}

vec2 sdf_grad(vec2 uv, float type_val, float seed, float step_uv) {
  step_uv = quant_step(step_uv);
  vec2 e = vec2(step_uv, 0.0);
  float sx1 = plant_sdf(uv + e.xy, type_val, seed);
  float sx2 = plant_sdf(uv - e.xy, type_val, seed);
  float sy1 = plant_sdf(uv + e.yx, type_val, seed);
  float sy2 = plant_sdf(uv - e.yx, type_val, seed);
  return vec2(sx1 - sx2, sy1 - sy2) * (0.5 / step_uv);
}

void main() {

  float dryness = mix(0.35, 0.92, h11(v_seed * 2.7 + v_type * 0.73));
  vec3 lush = vec3(0.17, 0.32, 0.19);
  vec3 dry = vec3(0.46, 0.44, 0.28);
  vec3 base = mix(lush, dry, dryness);
  base = mix(base, v_color, 0.40);
  base *= 0.88;

  vec2 uv2 = (v_tex_coord - 0.5) * 2.0;
  float r2 = clamp(dot(uv2, uv2), 0.0, 1.0);
  float z = sqrt(max(1.0 - r2, 0.0));
  vec3 Nbulge =
      normalize(v_tangent * uv2.x + v_bitangent * uv2.y + v_normal * (z * 1.8));
  vec3 N = normalize(mix(v_normal, Nbulge, 0.85));

  float type_val = fract(v_type);
  float sdf = plant_sdf(v_tex_coord, type_val, v_seed);
  if (sdf > -0.004)
    discard;

  float step_uv = aawidth_uv(v_tex_coord);
  vec2 g = sdf_grad(v_tex_coord, type_val, v_seed, step_uv);
  vec3 Nshape = normalize(v_tangent * (-g.x) + v_bitangent * (-g.y) + v_normal * 3.0);
  float edge_mix = smoothstep(0.30, 0.0, sdf);
  vec3 Ntemp = normalize(mix(N, Nshape, 0.6 * edge_mix));

  float w_aa = aawidth_uv(v_tex_coord);
  float edge1 = 1.0 - smoothstep(-w_aa, w_aa, sdf);
  N = normalize(mix(Ntemp, v_normal, edge1 * 0.5));
  float edge_atten = mix(0.6, 1.0, pow(1.0 - edge1, 1.5));

  vec3 L = normalize(u_light_direction);
  float nl = max(dot(N, L), 0.0);
  float half_lambert = nl * 0.5 + 0.5;
  float wrap = clamp((dot(N, L) + 0.20) / 1.20, 0.0, 1.0);
  float diffuse = mix(half_lambert, wrap, 0.30) * edge_atten;
  float sss = pow(clamp(dot(-N, L), 0.0, 1.0), 2.2) * 0.22 * edge_atten;
  float ambient = 0.16;

  float ao_stem = mix(0.50, 1.0, smoothstep(0.0, 0.55, v_height));

  float tip = smoothstep(0.25, 1.0, r2);
  float inner = smoothstep(-2.0 * w_aa, -0.2 * w_aa, sdf);
  vec3 albedo = base;
  albedo *= mix(1.0, 1.08, tip);
  albedo *= mix(0.95, 1.0, inner);

  vec3 color =
      albedo * (ambient + diffuse * ao_stem) + albedo * sss * vec3(1.0, 0.95, 0.85);

  frag_color = vec4(color, 1.0);
}
