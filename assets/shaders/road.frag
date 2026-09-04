#version 330 core
#include "directional_shadows.glsl"
#include "environment_lighting.glsl"
#include "local_lighting.glsl"
#include "visibility_mask.glsl"

in vec3 v_normal;
in vec2 v_tex_coord;
in vec3 v_world_pos;

uniform vec3 u_color;
uniform float u_alpha;
uniform int u_surface_kind;
uniform vec3 u_camera_pos;

out vec4 frag_color;

const float PI = 3.14159265359;

float saturate_val(float x) {
  return clamp(x, 0.0, 1.0);
}

mat2 rotate_2d(float a) {
  float c = cos(a), s = sin(a);
  return mat2(c, -s, s, c);
}

float hash_2d(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

float noise_2d(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float a = hash_2d(i);
  float b = hash_2d(i + vec2(1.0, 0.0));
  float c = hash_2d(i + vec2(0.0, 1.0));
  float d = hash_2d(i + vec2(1.0, 1.0));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm_2d(vec2 p) {
  float v = 0.0, a = 0.5;
  for (int i = 0; i < SOI_TERRAIN_NOISE_OCTAVES; ++i) {
    v += a * noise_2d(p);
    p *= 2.0;
    a *= 0.5;
  }
  return v;
}

vec2 hash_2d_vec(vec2 p) {
  float n = sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123;
  return fract(vec2(n, n * 1.2154));
}

vec2 worley_f(vec2 p) {
  vec2 n = floor(p);
  vec2 f = fract(p);
  float f1 = 1e9;
  float f2 = 1e9;
  for (int j = -1; j <= 1; ++j) {
    for (int i = -1; i <= 1; ++i) {
      vec2 g = vec2(float(i), float(j));
      vec2 o = hash_2d_vec(n + g);
      vec2 r = (g + o) - f;
      float d = dot(r, r);
      if (d < f1) {
        f2 = f1;
        f1 = d;
      } else if (d < f2) {
        f2 = d;
      }
    }
  }
  return vec2(sqrt(f1), sqrt(f2));
}

float fresnel_schlick(float cos_theta, float f0) {
  return f0 + (1.0 - f0) * pow(1.0 - cos_theta, 5.0);
}

float ggx_specular(vec3 n_vec, vec3 v_vec, vec3 l_vec, float rough, float f0) {
  vec3 h_vec = normalize(v_vec + l_vec);
  float n_dot_v = max(dot(n_vec, v_vec), 0.0);
  float n_dot_l = max(dot(n_vec, l_vec), 0.0);
  float n_dot_h = max(dot(n_vec, h_vec), 0.0);
  float v_dot_h = max(dot(v_vec, h_vec), 0.0);

  float a = max(rough * rough, 0.001);
  float a2 = a * a;
  float denom = (n_dot_h * n_dot_h * (a2 - 1.0) + 1.0);
  float d_val = a2 / max(PI * denom * denom, 1e-4);

  float k = (a + 1.0);
  k = (k * k) * 0.125;
  float g_v = n_dot_v / (n_dot_v * (1.0 - k) + k);
  float g_l = n_dot_l / (n_dot_l * (1.0 - k) + k);
  float g_val = g_v * g_l;

  float f_val = fresnel_schlick(v_dot_h, f0);
  return (d_val * g_val * f_val) / max(4.0 * n_dot_v * n_dot_l, 1e-4);
}

void main() {
  vec2 uv = v_world_pos.xz;
  float across = clamp(v_tex_coord.x, 0.0, 1.0);

  float boundary = clamp(v_tex_coord.y, 0.0, 1.0);
  float edge_noise = fbm_2d(uv * 0.42 + vec2(19.0, -7.0)) - 0.5;
  float edge_alpha =
      smoothstep(0.02, 0.92, clamp(boundary + edge_noise * 0.34, 0.0, 1.0));
  float edge_distance = boundary * 0.5;

  float broad = fbm_2d(uv * 0.055 + vec2(7.0, 13.0));
  float medium = fbm_2d(uv * 0.34 + vec2(-11.0, 3.0));
  float grain = noise_2d(uv * 3.7 + vec2(29.0, -17.0));
  float road_age = fbm_2d(uv * 0.018 + vec2(-37.0, 23.0));
  float rut_wander = (noise_2d(uv * 0.075 + vec2(41.0, -9.0)) - 0.5) * 0.035;
  float rut_left = 1.0 - smoothstep(0.030, 0.090, abs(across - (0.31 + rut_wander)));
  float rut_right = 1.0 - smoothstep(0.030, 0.090, abs(across - (0.69 + rut_wander)));
  float ruts = max(rut_left, rut_right);
  float traveled = smoothstep(0.24, 0.68, broad * 0.70 + medium * 0.30);
  ruts *= mix(0.35, 1.0, traveled);
  float center_wear = 1.0 - smoothstep(0.08, 0.48, abs(across - 0.5));

  vec3 base_color;
  float h;
  float ao;
  float material_roughness;

  if (u_surface_kind == 2) {
    vec2 stone_warp = vec2(noise_2d(uv * 0.17 + vec2(17.0, -31.0)),
                           noise_2d(uv * 0.17 + vec2(-43.0, 11.0))) -
                      0.5;
    vec2 stone_uv = uv * 2.05 + stone_warp * 0.42;
    vec2 worley_result = worley_f(stone_uv);
    float edge_metric = worley_result.y - worley_result.x;
    float edge_aa = max(fwidth(edge_metric) * 1.35, 0.008);
    float stone_mask = smoothstep(0.035 - edge_aa, 0.125 + edge_aa, edge_metric);
    float mortar_mask = 1.0 - stone_mask;
    float stone_variation =
        (broad - 0.5) * 0.11 + (medium - 0.5) * 0.08 + (grain - 0.5) * 0.045;
    float stone_face = 1.0 - smoothstep(0.08, 0.48, worley_result.x);
    vec3 stone_color = u_color * (0.92 + stone_variation + stone_face * 0.035);
    vec3 mortar_color = mix(u_color * 0.54, vec3(0.20, 0.17, 0.13), 0.18);
    base_color = mix(mortar_color, stone_color, stone_mask);
    float cavity = smoothstep(0.0, 0.18, edge_metric);
    ao = mix(0.64, 1.0, cavity);
    h = (medium - 0.5) * 0.024 * stone_mask - mortar_mask * 0.032;
    material_roughness = 0.90;
  } else if (u_surface_kind == 1) {
    vec3 dry_earth = u_color * 1.02;
    vec3 compressed_earth = u_color * 0.78;
    float compacted = clamp(ruts * 0.48 + center_wear * 0.10, 0.0, 1.0);
    base_color = mix(dry_earth, compressed_earth, compacted);

    vec2 gravel_cells = worley_f(uv * 3.1 + vec2(-11.0, 6.0));
    float gravel = smoothstep(0.035, 0.16, gravel_cells.y - gravel_cells.x);
    base_color = mix(base_color, base_color * 1.17, gravel * 0.55);
    base_color *= 0.88 + broad * 0.12 + medium * 0.07;
    h = (medium - 0.5) * 0.050 - ruts * 0.042 + gravel * 0.018;
    ao = 0.92 - ruts * 0.11 - (1.0 - medium) * 0.06;
    material_roughness = 0.97;
  } else {

    vec2 aggregate_cells = worley_f(uv * 1.85 + vec2(3.0, -8.0));
    float aggregate_edge = aggregate_cells.y - aggregate_cells.x;
    float aggregate = smoothstep(0.020, 0.110, aggregate_edge);
    float aggregate_tone = (noise_2d(uv * 1.85 + vec2(-5.0, 17.0)) - 0.5) * 0.16;
    float wear = center_wear * smoothstep(0.30, 0.68, broad);
    float shoulder = 1.0 - smoothstep(0.055, 0.20, edge_distance);
    vec3 joint_earth = u_color * (0.76 + broad * 0.10);
    vec3 packed_stone = mix(u_color * 1.06, vec3(0.43, 0.39, 0.31), 0.14) *
                        (0.94 + aggregate_tone + (medium - 0.5) * 0.07);
    base_color = mix(joint_earth, packed_stone, aggregate);
    base_color *= 1.0 - wear * 0.045 - shoulder * 0.10;
    h = (medium - 0.5) * 0.030 + aggregate * 0.032 - wear * 0.010;
    ao = 0.88 + aggregate * 0.10 - wear * 0.018 - shoulder * 0.035;
    material_roughness = 0.94;
  }

  float shoulder_grit =
      1.0 - smoothstep(0.045, 0.19, edge_distance + edge_noise * 0.025);
  vec3 shoulder_color = mix(base_color * 0.70, u_color * 0.58, 0.34);
  base_color = mix(base_color, shoulder_color, shoulder_grit * 0.48);
  base_color *= 0.94 + (road_age - 0.5) * 0.10;
  base_color *= 1.0 - ruts * mix(0.025, 0.075, environment_wetness());

  float sx = dFdx(h);
  float sy = dFdy(h);
  float bump_strength = 6.5;
  vec3 n_bump = normalize(vec3(-sx * bump_strength, 1.0, -sy * bump_strength));

  vec3 n_geom = normalize(v_normal);
  vec3 n_final = normalize(mix(n_geom, n_bump, 0.38));

  vec3 light_dir = environment_primary_direction();
  vec3 view_dir = normalize(u_camera_pos - v_world_pos);

  float steep = saturate_val(length(vec2(sx, sy)) * bump_strength);
  float wetness = environment_wetness();
  float puddle_field = noise_2d(uv * 0.55 + vec2(13.0, 5.0)) * 0.6 +
                       noise_2d(uv * 1.7 + vec2(-3.0, 21.0)) * 0.4;
  float hollow = clamp(-h * 14.0 + ruts * 0.55, 0.0, 1.0);
  float puddle = smoothstep(0.62, 0.80, puddle_field + hollow * 0.20) * wetness;
  float damp = wetness * (0.55 + 0.45 * hollow);
  base_color *= 1.0 - damp * 0.34 - puddle * 0.30;
  vec3 n_wet = normalize(mix(n_final, n_geom, puddle * 0.92));
  float roughness =
      clamp(material_roughness + steep * 0.08 - damp * 0.30 - puddle * 0.45, 0.10, 1.0);
  float f0 = mix(0.03, 0.05, wetness);

  float spec = ggx_specular(n_wet, view_dir, light_dir, roughness, f0);

  vec3 lit_color = base_color * soi_surface_lighting_scaled(n_final, 0.65) * ao;
  lit_color += environment_primary_color() * environment_primary_intensity() * spec *
               (0.20 + damp * 0.45 + puddle * 0.9);
  lit_color += environment_sky_color() * puddle * 0.10;

  float grime = (1.0 - ao) * 0.14 * (0.8 + 0.2 * noise_2d(uv * 6.0));
  float gray = dot(lit_color, vec3(0.299, 0.587, 0.114));
  lit_color = mix(lit_color, vec3(gray * 0.85), grime);

  lit_color = apply_visibility_world_shading(lit_color, v_world_pos.xz);

  lit_color = apply_directional_shadow(lit_color, v_world_pos, v_normal);
  lit_color += base_color * ao * local_lighting(v_world_pos, normalize(v_normal));
  frag_color = vec4(lit_color, u_alpha * edge_alpha);
}
