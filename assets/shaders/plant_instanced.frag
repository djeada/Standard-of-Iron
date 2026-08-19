#version 330 core
#include "directional_shadows.glsl"
#include "environment_lighting.glsl"
#include "local_lighting.glsl"
#include "visibility_mask.glsl"

in vec3 v_normal;
in vec3 v_color;
in vec2 v_tex_coord;
in float v_alpha;
in float v_height;
flat in float v_seed;
flat in float v_type;
in vec3 v_tangent;
in vec3 v_bitangent;
in vec3 v_world_pos;

out vec4 frag_color;

float h11(float n) {
  return fract(sin(n) * 43758.5453123);
}

float pixel_width_uv(vec2 uv) {
  vec2 dx = dFdx(uv);
  vec2 dy = dFdy(uv);
  float w = 0.5 * (length(dx) + length(dy));
  return clamp(w, 0.00125, 0.012);
}

float segment_sdf(vec2 p, vec2 a, vec2 b, float r) {
  vec2 pa = p - a;
  vec2 ba = b - a;
  float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-5), 0.0, 1.0);
  return length(pa - ba * h) - r;
}

float bush_sdf(vec2 uv, float seed) {
  vec2 p = (uv - vec2(0.5, 0.46)) * vec2(1.12, 1.0);
  float sdf = 1e9;

  for (int i = 0; i < 7; i++) {
    float fi = float(i);
    float ang = fi * 0.897597901 + seed * 3.7;
    float spread = 0.20 + h11(seed * 7.9 + fi) * 0.11;
    vec2 c = vec2(cos(ang), sin(ang) * 0.84) * spread + vec2(0.0, 0.05);
    float r = 0.145 + h11(seed * 5.7 + fi) * 0.085;
    sdf = min(sdf, length(p - c) - r);
  }

  float stem_lean = (h11(seed * 4.1) - 0.5) * 0.10;
  sdf = min(sdf, segment_sdf(p, vec2(stem_lean, -0.46), vec2(0.0, -0.04), 0.028));

  return sdf - 0.007;
}

float rosette_sdf(vec2 uv, float seed) {
  vec2 p = uv - 0.5;
  float a = atan(p.y, p.x);
  float r = length(p);

  float petals = mix(10.0, 16.0, h11(seed * 2.7));
  float wave = 0.20 + 0.06 * sin(a * petals + seed * 5.1);

  return r - wave - 0.006;
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
  step_uv = clamp(step_uv, 0.0015, 0.008);

  vec2 ex = vec2(step_uv, 0.0);
  vec2 ey = vec2(0.0, step_uv);

  float sx1 = plant_sdf(uv + ex, type_val, seed);
  float sx2 = plant_sdf(uv - ex, type_val, seed);
  float sy1 = plant_sdf(uv + ey, type_val, seed);
  float sy2 = plant_sdf(uv - ey, type_val, seed);

  return vec2(sx1 - sx2, sy1 - sy2) * (0.5 / step_uv);
}

void main() {
  float type_val = (mod(floor(v_type + 0.5), 4.0) + 0.5) * 0.25;
  float sdf = plant_sdf(v_tex_coord, type_val, v_seed);

  float sdf_aa = clamp(fwidth(sdf) * 0.85, 0.0015, 0.025);
  float coverage = 1.0 - smoothstep(-sdf_aa, sdf_aa, sdf);
  coverage *= v_alpha;

  if (coverage <= 0.003)
    discard;

  float dryness = mix(0.35, 0.92, h11(v_seed * 2.7 + v_type * 0.73));

  vec3 lush = vec3(0.17, 0.32, 0.18);
  vec3 dry = vec3(0.38, 0.36, 0.24);

  vec3 base = mix(lush, dry, dryness);
  base = mix(base, v_color, 0.40);
  base *= 0.94;

  base = mix(base, base * vec3(1.22, 1.16, 0.86), smoothstep(0.35, 1.0, v_height));

  vec2 uv2 = (v_tex_coord - 0.5) * 2.0;
  float r2 = clamp(dot(uv2, uv2), 0.0, 1.0);
  float z = sqrt(max(1.0 - r2, 0.0));

  vec3 Nbulge =
      normalize(v_tangent * uv2.x + v_bitangent * uv2.y + v_normal * (z * 1.8));

  vec3 N = normalize(mix(v_normal, Nbulge, 0.85));

  float grad_step = max(pixel_width_uv(v_tex_coord) * 1.5, sdf_aa);
  vec2 g = sdf_grad(v_tex_coord, type_val, v_seed, grad_step);

  vec3 Nshape = normalize(v_tangent * (-g.x) + v_bitangent * (-g.y) + v_normal * 3.0);

  float rim = 1.0 - smoothstep(0.0, 0.14, -sdf);

  vec3 Ntemp = normalize(mix(N, Nshape, 0.65 * rim));
  N = normalize(mix(Ntemp, v_normal, 0.25 * rim));

  float edge_atten = mix(1.0, 0.72, rim);

  vec3 L = environment_primary_direction();

  float nl = max(dot(N, L), 0.0);
  float half_lambert = nl * 0.5 + 0.5;
  float wrap = clamp((dot(N, L) + 0.20) / 1.20, 0.0, 1.0);

  float diffuse = mix(half_lambert, wrap, 0.30) * edge_atten;
  float sss = pow(clamp(dot(-N, L), 0.0, 1.0), 2.2) * 0.22 * edge_atten;

  float ao_stem = mix(0.50, 1.0, smoothstep(0.0, 0.55, v_height));

  float tip = smoothstep(0.25, 1.0, r2);
  float inner = smoothstep(-2.0 * sdf_aa, -0.2 * sdf_aa, sdf);

  vec3 albedo = base;
  albedo *= mix(1.0, 1.08, tip);
  albedo *= mix(0.95, 1.0, inner);

  float leaf_angle = atan(uv2.y, uv2.x);
  float vein_count = mix(5.0, 9.0, h11(v_seed * 8.1));
  float leaf_wave = sin(leaf_angle * vein_count + v_seed * 6.0);
  float radial = smoothstep(0.08, 0.80, sqrt(r2));
  float lobe_shade = (0.5 - 0.5 * cos(leaf_angle * vein_count + v_seed * 6.0)) * radial;
  float crease = pow(abs(leaf_wave), 24.0) * radial;
  albedo *= mix(1.0, 0.88, lobe_shade);
  albedo *= mix(1.0, 0.66, crease * 0.55);

  float blemish = h11(floor(v_world_pos.x * 19.0 + v_world_pos.z * 23.0) +
                      floor(v_tex_coord.y * 17.0) + v_seed * 31.0);
  float leaf_spot = smoothstep(0.965, 0.995, blemish) * (1.0 - rim);
  albedo = mix(albedo, albedo * vec3(0.48, 0.42, 0.30), leaf_spot * 0.55);

  vec3 sun = environment_primary_color() * environment_primary_intensity();
  vec3 illumination = environment_ambient_light(N) + sun * diffuse * ao_stem;
  vec3 color = (albedo * illumination + albedo * sss * sun) * environment_exposure();

  color = apply_directional_shadow(color, v_world_pos, v_normal);
  color += albedo * local_lighting(v_world_pos, normalize(v_normal));
  color = apply_visibility_memory(color, v_world_pos.xz);
  frag_color = vec4(color, coverage);
}
