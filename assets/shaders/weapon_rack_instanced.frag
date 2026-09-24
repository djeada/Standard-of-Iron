#version 330 core
#include "directional_shadows.glsl"
#include "environment_lighting.glsl"
#include "local_lighting.glsl"
#include "noise.glsl"
#include "visibility_mask.glsl"

in vec3 v_world_pos;
in vec3 v_normal;
in vec3 v_color;
in vec3 v_local_pos;
in vec3 v_local_normal;
in vec2 v_uv;
flat in float v_material;
flat in float v_seed;
flat in float v_instance_hash;

uniform vec3 u_camera_pos;

out vec4 frag_color;

const int k_mat_oak = 0;
const int k_mat_ash = 1;
const int k_mat_steel = 2;
const int k_mat_iron = 3;
const int k_mat_bronze = 4;
const int k_mat_leather = 5;
const int k_mat_yew = 6;
const int k_mat_linen = 7;
const int k_mat_shield_paint = 8;
const int k_mat_shield_back = 9;
const int k_mat_bone = 10;
const int k_mat_feather = 11;

const float k_tau = 6.2831853;
const float k_superellipse = 2.4;

struct RackSurface {
  vec3 albedo;
  float metallic;
  float roughness;
  float height;
  float bump;
  float cavity;
};

float band(float value, float center, float half_width, float feather) {
  return 1.0 - smoothstep(half_width, half_width + feather, abs(value - center));
}

float range_mask(float value, float lo, float hi, float feather) {
  return smoothstep(lo - feather, lo + feather, value) *
         (1.0 - smoothstep(hi - feather, hi + feather, value));
}

float luminance(vec3 c) {
  return dot(c, vec3(0.299, 0.587, 0.114));
}

float ground_grime() {
  return 1.0 - smoothstep(0.0, 0.30, v_local_pos.y);
}

RackSurface
timber(vec3 base, vec2 uv, float seed, float ring_density, float warp_gain) {
  float warp = soi_fbm_23e5ab(vec2(uv.x * 4.0, uv.y * 0.9) + seed * 11.0);
  float rings = fract(uv.x * ring_density + warp * warp_gain + seed * 3.0);
  float latewood = smoothstep(0.58, 0.94, rings);
  float fibre = soi_noise2(vec2(uv.x * 220.0, uv.y * 9.0) + seed * 31.0);
  float rays =
      smoothstep(0.90, 0.985, soi_noise2(vec2(uv.x * 70.0, uv.y * 16.0) + seed * 7.0));
  float check =
      smoothstep(0.972, 0.995, soi_noise2(vec2(uv.x * 95.0 + seed * 5.0, uv.y * 1.6)));

  vec3 early = base * vec3(1.20, 1.09, 0.94);
  vec3 late = base * vec3(0.60, 0.52, 0.45);
  vec3 c = mix(early, late, latewood * 0.72);
  c *= mix(0.86, 1.07, fibre);
  c = mix(c, base * vec3(1.32, 1.20, 1.02), rays * 0.35);
  c *= 1.0 - check * 0.55;

  float exposure = clamp(v_local_normal.y, 0.0, 1.0) * 0.6 + 0.25;
  float silver =
      clamp(exposure + (soi_fbm_23e5ab(uv * vec2(3.0, 0.7) + seed * 19.0) - 0.5) * 0.6,
            0.0,
            1.0);
  vec3 grey = vec3(luminance(c)) * vec3(1.08, 1.03, 0.96) * 1.12;
  c = mix(c, grey, silver * 0.38);
  c = mix(c, c * vec3(0.56, 0.50, 0.43), ground_grime() * 0.75);

  RackSurface s;
  s.albedo = c;
  s.metallic = 0.0;
  s.roughness = mix(0.70, 0.86, latewood);
  s.height = -latewood * 0.0010 - (1.0 - fibre) * 0.00035 - check * 0.0025;
  s.bump = 1.0;
  s.cavity = 1.0 - check * 0.5;
  return s;
}

RackSurface steel(vec2 uv, float seed) {
  float across = cos(uv.x * k_tau);
  float edge = smoothstep(0.62, 0.97, abs(across));
  float streak = soi_noise2(vec2(uv.x * 36.0 + seed * 9.0, uv.y * 260.0));
  float pit = soi_fbm_23e5ab(vec2(uv.x * 9.0, uv.y * 34.0) + seed * 5.0);
  float rust = smoothstep(0.64, 0.80, pit) * (1.0 - edge * 0.7);

  vec3 c = mix(vec3(0.56, 0.57, 0.59), vec3(0.80, 0.80, 0.82), edge);
  c *= mix(0.90, 1.05, streak);
  float temper = 1.0 - smoothstep(0.0, 0.10, uv.y);
  c = mix(c, vec3(0.60, 0.52, 0.40), temper * 0.30);
  c = mix(c, vec3(0.30, 0.13, 0.05), rust * 0.85);

  RackSurface s;
  s.albedo = c;
  s.metallic = mix(1.0, 0.15, rust);
  s.roughness = mix(mix(0.24, 0.13, edge) + streak * 0.08, 0.85, rust);
  s.height = rust * 0.0005 - streak * 0.00012;
  s.bump = 1.0;
  s.cavity = 1.0;
  return s;
}

RackSurface iron(vec2 uv, float seed) {
  float hammer = soi_noise2(uv * 55.0 + seed * 13.0);
  float scale = soi_fbm_23e5ab(uv * 7.0 + seed * 3.0);
  float rust = smoothstep(
      0.56,
      0.80,
      soi_fbm_23e5ab(vec2(uv.x * 6.0, uv.y * 2.5 + v_local_pos.y * 3.0) + seed * 17.0));
  rust = max(rust, ground_grime() * 0.6);

  vec3 c = mix(vec3(0.13, 0.13, 0.14), vec3(0.30, 0.30, 0.31), scale);
  c = mix(c, vec3(0.34, 0.16, 0.07), rust * 0.9);

  RackSurface s;
  s.albedo = c;
  s.metallic = mix(0.85, 0.08, rust);
  s.roughness = mix(0.42 + hammer * 0.22, 0.92, rust);
  s.height = -hammer * 0.0008 + rust * 0.0004;
  s.bump = 1.0;
  s.cavity = 1.0;
  return s;
}

RackSurface bronze(float seed) {
  vec3 p = v_local_pos;
  float tarnish = soi_fbm_23e5ab(p.xy * 11.0 + p.zz * 7.0 + seed * 23.0);
  float patina_noise = soi_fbm_23e5ab(p.xz * 17.0 + p.yy * 9.0 + seed * 7.0);
  float underside = clamp(-v_local_normal.y * 0.6 + 0.3, 0.0, 1.0);
  float verdigris = smoothstep(0.55, 0.78, patina_noise + underside * 0.25);

  vec3 polished = vec3(0.86, 0.62, 0.30);
  vec3 dull = vec3(0.50, 0.32, 0.14);
  vec3 c = mix(polished, dull, smoothstep(0.35, 0.75, tarnish));
  c = mix(c, vec3(0.25, 0.50, 0.40), verdigris * 0.85);

  RackSurface s;
  s.albedo = c;
  s.metallic = mix(1.0, 0.0, verdigris);
  s.roughness = mix(mix(0.22, 0.48, tarnish), 0.86, verdigris);
  s.height = verdigris * 0.0005 - tarnish * 0.0002;
  s.bump = 1.0;
  s.cavity = 1.0 - verdigris * 0.25;
  return s;
}

RackSurface leather(vec2 uv, float seed) {
  float pebble = soi_noise2(uv * vec2(95.0, 140.0) + seed * 29.0);
  float scuff =
      smoothstep(0.62, 0.86, soi_fbm_23e5ab(uv * vec2(5.0, 9.0) + seed * 3.0));
  vec3 base = mix(vec3(0.24, 0.12, 0.055), vec3(0.44, 0.24, 0.10), fract(seed * 7.13));
  vec3 c = base * mix(0.82, 1.08, pebble);
  c = mix(c, base * vec3(1.55, 1.40, 1.20), scuff * 0.45);
  c = mix(c, c * vec3(0.60, 0.55, 0.50), ground_grime() * 0.6);

  RackSurface s;
  s.albedo = c;
  s.metallic = 0.0;
  s.roughness = mix(0.46, 0.70, scuff);
  s.height = pebble * 0.0006;
  s.bump = 1.0;
  s.cavity = 1.0;
  return s;
}

RackSurface yew(vec2 uv, float seed) {
  float back = sin(uv.x * k_tau);
  float streak = soi_noise2(vec2(uv.x * 12.0, uv.y * 40.0) + seed * 5.0);
  float sap = smoothstep(0.10, 0.45, back + (streak - 0.5) * 0.25);
  float grain = soi_noise2(vec2(uv.x * 30.0, uv.y * 220.0) + seed * 11.0);
  vec3 heart = vec3(0.56, 0.24, 0.08);
  vec3 sapwood = vec3(0.84, 0.70, 0.46);
  vec3 c = mix(heart, sapwood, sap) * mix(0.86, 1.06, grain);

  RackSurface s;
  s.albedo = c;
  s.metallic = 0.0;
  s.roughness = 0.34;
  s.height = -grain * 0.0003;
  s.bump = 1.0;
  s.cavity = 1.0;
  return s;
}

RackSurface linen(vec2 uv, float seed) {
  float lay = 0.5 + 0.5 * sin(uv.y * 420.0 + uv.x * k_tau * 3.0 + seed * 9.0);
  vec3 c = vec3(0.80, 0.75, 0.62) * mix(0.76, 1.06, lay);
  c *= mix(0.9, 1.0, soi_noise2(uv * vec2(8.0, 60.0) + seed));

  RackSurface s;
  s.albedo = c;
  s.metallic = 0.0;
  s.roughness = 0.9;
  s.height = lay * 0.0004;
  s.bump = 1.0;
  s.cavity = mix(0.8, 1.0, lay);
  return s;
}

vec3 weather_paint(vec3 paint, vec2 uv, float seed, float rim_weight, out float chip) {
  float chips = soi_fbm_23e5ab(uv * vec2(9.0, 7.0) + seed * 13.0);
  chip = smoothstep(0.66, 0.80, chips) * (0.30 + 0.70 * rim_weight);
  float scratch_line =
      soi_noise2(vec2(uv.x * 6.0 + uv.y * 70.0, uv.y * 3.0) + seed * 3.0);
  float scratch = smoothstep(0.93, 0.99, scratch_line);
  vec3 hide = vec3(0.42, 0.31, 0.20);
  vec3 c = paint * mix(0.88, 1.06, soi_fbm_23e5ab(uv * 4.0 + seed * 7.0));
  c = mix(c, hide, max(chip, scratch * 0.6));
  return c;
}

RackSurface scutum_face(vec2 uv, float seed) {
  float s = uv.x;
  float t = uv.y;
  float e = pow(pow(abs(s), k_superellipse) + pow(abs(t), k_superellipse),
                1.0 / k_superellipse);

  float palette = fract(v_instance_hash * 3.7 + seed);
  vec3 field = palette < 0.55   ? vec3(0.46, 0.055, 0.035)
               : palette < 0.85 ? vec3(0.56, 0.14, 0.055)
                                : vec3(0.18, 0.20, 0.30);
  vec3 motif = palette < 0.85 ? vec3(0.80, 0.58, 0.20) : vec3(0.84, 0.78, 0.62);

  float border = band(e, 0.885, 0.024, 0.008);
  float hairline = band(e, 0.815, 0.0055, 0.005);

  float at = abs(t);
  float zig = (abs(fract(at * 5.5) - 0.5) * 4.0 - 1.0) * 0.045;
  float bolt =
      band(s, zig, 0.026 * (1.25 - at), 0.010) * range_mask(at, 0.21, 0.74, 0.01);
  float bolt_tip = band(s, 0.0, max(0.84 - at, 0.0) * 0.55, 0.008) *
                   range_mask(at, 0.72, 0.85, 0.008);

  vec2 q = vec2(abs(s), at);
  float wings = 0.0;
  for (int k = 0; k < 3; ++k) {
    float kf = float(k);
    float curve = 0.13 + (0.10 + 0.13 * kf) * pow(max(q.x - 0.10, 0.0), 1.15);
    wings = max(wings,
                band(q.y, curve, 0.011, 0.006) *
                    range_mask(q.x, 0.15, 0.56 - kf * 0.06, 0.02));
  }

  float motif_mask =
      clamp(max(max(border, hairline), max(max(bolt, bolt_tip), wings)), 0.0, 1.0);
  vec3 paint = mix(field, motif, motif_mask);

  float chip;
  vec3 c = weather_paint(paint, uv, seed, smoothstep(0.62, 1.0, e), chip);
  float dirt = smoothstep(-0.40, -1.0, t);
  c = mix(c, vec3(0.30, 0.24, 0.17), dirt * 0.55);

  RackSurface r;
  r.albedo = c;
  r.metallic = 0.0;
  r.roughness = mix(0.52, 0.80, max(chip, dirt));
  r.height = motif_mask * 0.00025 - chip * 0.0009;
  r.bump = 1.0;
  r.cavity = 1.0;
  return r;
}

RackSurface parma_face(vec2 uv, float seed) {
  float r = length(uv);
  float ang = atan(uv.y, uv.x);
  float palette = fract(v_instance_hash * 5.3 + seed);
  vec3 field = palette < 0.6 ? vec3(0.70, 0.58, 0.38) : vec3(0.15, 0.24, 0.25);
  vec3 accent = vec3(0.52, 0.10, 0.05);
  vec3 light = vec3(0.86, 0.80, 0.64);

  float ring = range_mask(r, 0.70, 0.86, 0.008);
  float studs =
      band(fract(ang / k_tau * 18.0), 0.5, 0.16, 0.06) * band(r, 0.78, 0.028, 0.01);
  float star_r = 0.36 * (0.40 + 0.60 * pow(abs(cos(ang * 4.0)), 3.0));
  float star = 1.0 - smoothstep(star_r - 0.012, star_r + 0.012, r);

  vec3 paint = mix(field, accent, max(ring, star));
  paint = mix(paint, light, studs);

  float chip;
  vec3 c = weather_paint(paint, uv, seed, smoothstep(0.6, 1.0, r), chip);
  c = mix(c, vec3(0.30, 0.24, 0.17), smoothstep(-0.30, -1.0, uv.y) * 0.45);

  RackSurface s;
  s.albedo = c;
  s.metallic = 0.0;
  s.roughness = mix(0.55, 0.82, chip);
  s.height = studs * 0.0006 - chip * 0.0009;
  s.bump = 1.0;
  s.cavity = 1.0;
  return s;
}

RackSurface shield_back(vec2 uv, float seed) {
  float strip = fract(uv.x * 7.0 + seed);
  float seam = band(strip, 0.0, 0.03, 0.02) + band(strip, 1.0, 0.03, 0.02);
  float grain = soi_noise2(vec2(uv.x * 60.0, uv.y * 6.0) + seed * 9.0);
  vec3 c = vec3(0.38, 0.28, 0.18) * mix(0.82, 1.06, grain) * (1.0 - seam * 0.35);

  RackSurface s;
  s.albedo = c;
  s.metallic = 0.0;
  s.roughness = 0.85;
  s.height = -seam * 0.0008;
  s.bump = 1.0;
  s.cavity = 1.0 - seam * 0.3;
  return s;
}

RackSurface bone(vec2 uv, float seed) {
  float age = soi_fbm_23e5ab(uv * vec2(6.0, 20.0) + seed * 7.0);
  float crack =
      smoothstep(0.965, 0.995, soi_noise2(vec2(uv.x * 18.0, uv.y * 90.0) + seed));
  vec3 c = mix(vec3(0.88, 0.82, 0.68), vec3(0.70, 0.60, 0.42), age * 0.7);
  c *= 1.0 - crack * 0.45;

  RackSurface s;
  s.albedo = c;
  s.metallic = 0.0;
  s.roughness = 0.40;
  s.height = -crack * 0.0005;
  s.bump = 1.0;
  s.cavity = 1.0;
  return s;
}

RackSurface feather(vec2 uv, float seed) {
  float bar = smoothstep(0.35, 0.65, fract(uv.y * 70.0 + seed * 3.0));
  vec3 grey = mix(vec3(0.82, 0.80, 0.74), vec3(0.30, 0.27, 0.23), bar * 0.75);
  vec3 dyed = vec3(0.62, 0.12, 0.06);
  vec3 c = fract(seed * 13.7) > 0.66 ? mix(grey, dyed, 0.7) : grey;

  RackSurface s;
  s.albedo = c;
  s.metallic = 0.0;
  s.roughness = 0.8;
  s.height = 0.0;
  s.bump = 0.0;
  s.cavity = 1.0;
  return s;
}

RackSurface rack_surface(int material) {
  if (material == k_mat_ash) {
    vec3 ash = mix(vec3(0.62, 0.48, 0.31), vec3(0.52, 0.38, 0.22), fract(v_seed * 5.1));
    RackSurface s = timber(ash, v_uv * vec2(0.20, 1.0), v_seed, 18.0, 0.6);
    float handled = range_mask(v_uv.y, 0.72, 1.18, 0.10);
    s.albedo *= mix(1.0, 0.72, handled);
    s.roughness = mix(s.roughness * 0.75, 0.42, handled);
    return s;
  }
  if (material == k_mat_steel) {
    return steel(v_uv, v_seed);
  }
  if (material == k_mat_iron) {
    return iron(v_uv, v_seed);
  }
  if (material == k_mat_bronze) {
    return bronze(v_seed);
  }
  if (material == k_mat_leather) {
    return leather(v_uv, v_seed);
  }
  if (material == k_mat_yew) {
    return yew(v_uv, v_seed);
  }
  if (material == k_mat_linen) {
    return linen(v_uv, v_seed);
  }
  if (material == k_mat_shield_paint) {
    if (v_seed < 0.5) {
      return scutum_face(v_uv, v_seed);
    }
    return parma_face(v_uv, v_seed);
  }
  if (material == k_mat_shield_back) {
    return shield_back(v_uv, v_seed);
  }
  if (material == k_mat_bone) {
    return bone(v_uv, v_seed);
  }
  if (material == k_mat_feather) {
    return feather(v_uv, v_seed);
  }
  return timber(v_color, v_uv, v_seed, 26.0, 2.4);
}

vec3 perturb_normal(vec3 n, vec3 p, float h, float strength) {
  vec3 dpdx = dFdx(p);
  vec3 dpdy = dFdy(p);
  float dhdx = dFdx(h);
  float dhdy = dFdy(h);
  vec3 r1 = cross(dpdy, n);
  vec3 r2 = cross(n, dpdx);
  float det = dot(dpdx, r1);
  vec3 grad = sign(det) * (dhdx * r1 + dhdy * r2);
  return normalize(abs(det) * n - strength * grad);
}

vec3 rack_environment(vec3 r, float roughness) {
  vec3 sky = environment_sky_color();
  vec3 ground = environment_ground_bounce_color();
  vec3 horizon = mix(sky, environment_fog_color(), 0.5) * 1.12;
  float blur = mix(0.10, 0.80, roughness);
  vec3 upper = mix(horizon, sky * 0.92, smoothstep(0.0, 0.7 + blur, r.y));
  vec3 lower = mix(horizon * 0.8, ground * 0.65, smoothstep(0.0, 0.3 + blur, -r.y));
  vec3 env = r.y >= 0.0 ? upper : lower;
  return env * (environment_ambient_intensity() + 0.25) * environment_exposure();
}

float ggx_distribution(float n_dot_h, float roughness) {
  float a = roughness * roughness;
  float a2 = a * a;
  float d = n_dot_h * n_dot_h * (a2 - 1.0) + 1.0;
  return a2 / (3.14159265 * d * d);
}

float smith_visibility(float n_dot_l, float n_dot_v, float roughness) {
  float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
  float gl = n_dot_l / (n_dot_l * (1.0 - k) + k);
  float gv = n_dot_v / (n_dot_v * (1.0 - k) + k);
  return gl * gv / max(4.0 * n_dot_l * n_dot_v, 1.0e-3);
}

void main() {
  int material = int(v_material + 0.5);
  RackSurface surf = rack_surface(material);

  vec3 geometric_n = normalize(v_normal);
  vec3 V = normalize(u_camera_pos - v_world_pos);
  float view_distance = length(u_camera_pos - v_world_pos);
  float detail = 1.0 - smoothstep(16.0, 48.0, view_distance);
  vec3 N = perturb_normal(geometric_n, v_world_pos, surf.height, surf.bump * detail);
  if (dot(N, V) < 0.0 && dot(geometric_n, V) < 0.0) {
    N = -N;
    geometric_n = -geometric_n;
  }

  vec3 L = environment_primary_direction();
  vec3 H = normalize(L + V);
  float n_dot_l = max(dot(N, L), 0.0);
  float n_dot_v = max(dot(N, V), 1.0e-3);
  float n_dot_h = max(dot(N, H), 0.0);
  float v_dot_h = max(dot(V, H), 0.0);
  float roughness = clamp(surf.roughness, 0.06, 1.0);

  float ground_ao = mix(0.52, 1.0, smoothstep(0.0, 0.42, v_local_pos.y));
  float depth_ao = mix(0.78, 1.0, smoothstep(-0.22, 0.08, v_local_pos.z));
  float ao = ground_ao * depth_ao * surf.cavity;

  vec3 f0 = mix(vec3(0.04), surf.albedo, surf.metallic);
  vec3 fresnel = f0 + (1.0 - f0) * pow(1.0 - v_dot_h, 5.0);
  vec3 diffuse_albedo = surf.albedo * (1.0 - surf.metallic);

  vec3 exposure_sun = environment_primary_color() * environment_primary_intensity() *
                      environment_exposure();
  vec3 key_diffuse = diffuse_albedo * soi_key_light(N) * environment_exposure();
  vec3 ambient =
      diffuse_albedo * environment_ambient_light(N) * environment_exposure() * ao;

  float spec_d = ggx_distribution(n_dot_h, roughness);
  vec3 sun_specular =
      exposure_sun * min(spec_d * smith_visibility(n_dot_l, n_dot_v, roughness), 24.0) *
      fresnel * n_dot_l;

  vec3 R = reflect(-V, N);
  vec3 env_fresnel =
      f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(1.0 - n_dot_v, 5.0);
  vec3 env_specular = rack_environment(R, roughness) * env_fresnel * ao;

  float shade = clamp(directional_shadow_occlusion(v_world_pos, geometric_n) *
                          environment_shadow_strength(),
                      0.0,
                      1.0);
  vec3 shadow_tint = mix(vec3(1.0), environment_shadow_tint(), shade);

  vec3 color = (key_diffuse + ambient + env_specular) * shadow_tint;
  color += sun_specular * (1.0 - shade);
  color += soi_rim_light(N, V) * mix(0.35, 0.9, surf.metallic) * ao;
  color += diffuse_albedo * ao * local_lighting(v_world_pos, N);
  color += local_lighting_specular(v_world_pos, N, V, mix(0.15, 1.2, 1.0 - roughness)) *
           mix(vec3(0.3), f0 * 2.0, surf.metallic);
  color = apply_visibility_revealed(color, v_world_pos.xz);
  frag_color = vec4(color, 1.0);
}
