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
flat in float v_seed;

uniform vec3 u_camera_pos;
uniform float u_time;
uniform float u_magic_strength;

out vec4 frag_color;

const vec3 k_ember_hot = vec3(2.30, 1.20, 0.46);
const vec3 k_ember = vec3(1.55, 0.36, 0.07);
const vec3 k_blood = vec3(0.78, 0.040, 0.028);
const vec3 k_abyss = vec3(0.24, 0.020, 0.20);
const vec3 k_ash = vec3(0.21, 0.195, 0.185);
const vec3 k_ochre_blood = vec3(0.30, 0.045, 0.030);
const vec3 k_bronze = vec3(0.34, 0.20, 0.085);
const vec3 k_verdigris = vec3(0.13, 0.25, 0.20);

float fbm(vec3 p) {
  float v = 0.0;
  float a = 0.5;
  for (int i = 0; i < 4; i++) {
    v += soi_noise3(p) * a;
    p = p * 2.03 + vec3(17.13, 7.91, 11.47);
    a *= 0.5;
  }
  return v;
}

vec3 hash33(vec3 p) {
  p = fract(p * vec3(0.1031, 0.11369, 0.13787));
  p += dot(p, p.yxz + 19.19);
  return fract(vec3((p.x + p.y) * p.z, (p.x + p.z) * p.y, (p.y + p.z) * p.x));
}

float cell_edge(vec3 p) {
  vec3 cell = floor(p);
  vec3 f = fract(p);
  float nearest = 8.0;
  float second = 8.0;
  for (int z = -1; z <= 1; z++) {
    for (int y = -1; y <= 1; y++) {
      for (int x = -1; x <= 1; x++) {
        vec3 offset = vec3(float(x), float(y), float(z));
        vec3 r = offset + hash33(cell + offset) - f;
        float d = dot(r, r);
        if (d < nearest) {
          second = nearest;
          nearest = d;
        } else if (d < second) {
          second = d;
        }
      }
    }
  }
  return sqrt(second) - sqrt(nearest);
}

float glow_line(float distance, float width) {
  float aa = max(fwidth(distance), 1.0e-4);
  float spread = max(width, aa * 1.5);
  return (1.0 - smoothstep(spread * 0.30, spread, distance)) * sqrt(width / spread);
}

float stroke(vec2 p, vec2 a, vec2 b, float width) {
  vec2 ab = b - a;
  float t = clamp(dot(p - a, ab) / dot(ab, ab), 0.0, 1.0);
  float distance = length(p - a - ab * t);
  float aa = max(fwidth(distance), 0.003);
  return 1.0 - smoothstep(width, width + aa, distance);
}

float tanit_sign(vec2 uv) {
  const float w = 0.009;
  float body = max(stroke(uv, vec2(0.0, 0.555), vec2(-0.115, 0.33), w),
                   stroke(uv, vec2(0.0, 0.555), vec2(0.115, 0.33), w));
  body = max(body, stroke(uv, vec2(-0.115, 0.33), vec2(0.115, 0.33), w));
  float arms = stroke(uv, vec2(-0.13, 0.568), vec2(0.13, 0.568), w);
  arms = max(arms, stroke(uv, vec2(-0.13, 0.568), vec2(-0.145, 0.62), w));
  arms = max(arms, stroke(uv, vec2(0.13, 0.568), vec2(0.145, 0.62), w));
  float ring = abs(length(uv - vec2(0.0, 0.632)) - 0.040);
  float head = 1.0 - smoothstep(w, w + max(fwidth(ring), 0.003), ring);
  return max(body, max(arms, head));
}

float inscription(vec2 uv, float seed) {
  vec2 cell = floor(uv);
  vec2 p = fract(uv);
  float choice = soi_hash13_1c8396(vec3(cell, seed * 11.0));
  float stem = stroke(p, vec2(0.5, 0.18), vec2(0.5, 0.82), 0.04);
  float upper = stroke(p, vec2(0.5, 0.76), vec2(0.80, 0.55), 0.04);
  float lower = stroke(p, vec2(0.5, 0.45), vec2(0.20, 0.66), 0.04);
  float foot = stroke(p, vec2(0.5, 0.20), vec2(0.78, 0.38), 0.04);
  return max(stem,
             max(upper, max(lower * step(0.35, choice), foot * step(0.65, choice)))) *
         step(0.58, choice);
}

vec3 relief_normal(vec3 n, float height) {
  vec3 dx = dFdx(v_world_pos);
  vec3 dy = dFdy(v_world_pos);
  vec3 a = cross(dy, n);
  vec3 b = cross(n, dx);
  float determinant = dot(dx, a);
  vec3 gradient = sign(determinant) * (dFdx(height) * a + dFdy(height) * b);
  if (abs(determinant) < 1.0e-12) {
    return n;
  }
  return normalize(abs(determinant) * n - gradient);
}

void main() {
  vec3 N = normalize(v_normal);
  vec3 L = environment_primary_direction();
  vec3 V = normalize(u_camera_pos - v_world_pos);
  vec3 H = normalize(L + V);

  vec3 lp = v_local_pos;
  vec3 ln = normalize(v_local_normal);
  vec3 p = lp * 1.12;
  float radial = length(lp.xz);
  float box_radius = max(abs(lp.x), abs(lp.z));
  float top_face = smoothstep(0.55, 0.85, ln.y);

  float platform = 1.0 - smoothstep(0.255, 0.275, lp.y);
  float altar_block =
      (1.0 - step(0.335, box_radius)) * step(0.235, lp.y) * (1.0 - step(0.945, lp.y));
  float needle =
      (1.0 - step(0.118, radial)) * step(0.885, lp.y) * (1.0 - step(1.84, lp.y));
  float bull_horns = step(1.40, lp.y) * (1.0 - step(0.075, abs(lp.z))) *
                     step(0.035, abs(lp.x)) * (1.0 - step(0.52, abs(lp.x)));
  float altar_horns = step(0.80, lp.y) * (1.0 - step(1.13, lp.y)) *
                      step(0.195, min(abs(lp.x), abs(lp.z))) *
                      (1.0 - step(0.37, box_radius));
  float bronze = clamp(max(bull_horns, altar_horns), 0.0, 1.0);
  float hearth = (1.0 - smoothstep(0.19, 0.225, radial)) * step(0.855, lp.y) *
                 (1.0 - step(0.945, lp.y)) * (1.0 - altar_horns);
  float monolith = (1.0 - platform) * (1.0 - altar_block) * (1.0 - needle) *
                   (1.0 - bronze) * step(0.34, box_radius);
  float obsidian = clamp(max(monolith, needle), 0.0, 1.0);

  float stone_large = fbm(p * 1.6 + vec3(3.0, 1.0, 6.0));
  float stone_grain = fbm(p * 7.4 + vec3(8.0, 2.0, 4.0));
  float weathering = fbm(p * 12.0 + vec3(v_seed * 7.0));

  vec3 basalt = v_color * mix(0.80, 1.50, stone_large);
  basalt *= mix(0.80, 1.16, stone_grain);
  float ash_dust = top_face * smoothstep(0.42, 0.72, weathering) * (1.0 - hearth) *
                   (1.0 - obsidian * 0.6);
  basalt = mix(basalt, k_ash, ash_dust * 0.60);
  float stain_field =
      fbm(vec3(lp.x * 6.0, lp.y * 1.6, lp.z * 6.0) + vec3(v_seed * 3.0));
  float blood_stain = smoothstep(0.56, 0.70, stain_field) * (1.0 - top_face) *
                      max(altar_block, monolith * (1.0 - smoothstep(0.30, 0.80, lp.y)));
  basalt = mix(basalt, k_ochre_blood, blood_stain * 0.75);
  basalt *= mix(0.62, 1.0, smoothstep(0.02, 0.30, lp.y));

  float patina = smoothstep(0.50, 0.72, fbm(p * 9.0 + vec3(2.0, v_seed * 5.0, 7.0)));
  vec3 bronze_color = mix(k_bronze, k_verdigris, patina * 0.75);
  bronze_color *= mix(0.70, 1.10, stone_grain);
  vec3 albedo = mix(basalt, bronze_color, bronze);

  float tanit_face = altar_block * step(0.245, box_radius) *
                     (1.0 - step(0.275, box_radius)) * step(0.26, lp.y) *
                     (1.0 - step(0.715, lp.y)) * (1.0 - top_face);
  vec2 face_uv = abs(ln.x) > abs(ln.z) ? vec2(lp.z * sign(ln.x), lp.y)
                                       : vec2(-lp.x * sign(ln.z), lp.y);
  float tanit = tanit_sign(face_uv) * tanit_face;

  vec2 glyph_uv = abs(ln.x) > abs(ln.z) ? lp.zy : lp.xy;
  float glyphs = inscription(glyph_uv * vec2(9.0, 7.0), v_seed) * monolith *
                 smoothstep(0.30, 0.45, lp.y) * (1.0 - smoothstep(1.05, 1.25, lp.y)) *
                 (1.0 - top_face);

  albedo *= 1.0 - max(tanit, glyphs) * 0.55;

  float grain_detail = 1.0 - smoothstep(0.04, 0.16, length(fwidth(p)));
  N = relief_normal(N,
                    (stone_grain * 0.004 - max(tanit, glyphs) * 0.003) * grain_detail);

  float flow = fbm(p * 2.6 + vec3(0.0, -u_time * 0.32, v_seed * 3.0));
  vec3 crack_warp = vec3(fbm(p * 2.2 + vec3(5.0)),
                         fbm(p * 2.2 + vec3(1.0, 9.0, 3.0)),
                         fbm(p * 2.2 + vec3(7.0, 2.0, 8.0)));
  float crack_edge = cell_edge(p * vec3(3.4, 2.2, 3.4) + (crack_warp - 0.5) * 1.6 +
                               vec3(v_seed * 9.0));
  float crack_live =
      smoothstep(0.38, 0.60, fbm(p * 1.9 + vec3(v_seed * 5.0, 2.0, 0.0)));

  float spoke_angle = atan(lp.z, lp.x) * 1.1140846;
  float spoke_wobble = fbm(vec3(radial * 3.0, 0.0, spoke_angle) + vec3(v_seed));
  float spoke = abs(fract(spoke_angle + spoke_wobble * 0.55) - 0.5) * radial * 0.8976;
  float spokes = glow_line(spoke, 0.022) * top_face * platform *
                 smoothstep(0.30, 0.36, radial) *
                 (1.0 - smoothstep(0.55, 0.86, radial));

  float base_heat = 1.0 - smoothstep(0.10, 1.45, lp.y);
  float monolith_heat = mix(0.35, 1.0, base_heat);
  float crack_core = glow_line(crack_edge, 0.030);
  float crack_halo = glow_line(crack_edge, 0.12);
  float fissure_mask = crack_live * (1.0 - bronze) * (1.0 - hearth) *
                       (platform * (1.0 - smoothstep(0.30, 0.90, radial)) * 1.15 +
                        altar_block * 0.75 + monolith * monolith_heat + needle * 0.55);
  float fissure = crack_core * fissure_mask;
  float scorch = crack_halo * fissure_mask;

  albedo *= 1.0 - scorch * 0.55;

  float needle_fire = needle * (1.0 - smoothstep(0.90, 1.30, lp.y));
  float lick =
      fbm(vec3(atan(lp.z, lp.x) * 1.4, lp.y * 7.0 - u_time * 2.6, v_seed * 4.0));
  float flame_reach = 0.95 + lick * 0.42;
  float flames = needle * (1.0 - smoothstep(flame_reach - 0.10, flame_reach, lp.y)) *
                 step(0.9, lp.y);

  float coal_field = fbm(p * 14.0 + vec3(0.0, u_time * 0.18, v_seed * 2.0));
  float coals = hearth * smoothstep(0.30, 0.62, coal_field);
  float flicker = 0.80 + 0.20 * sin(u_time * 7.3 + coal_field * 11.0) *
                             sin(u_time * 3.1 + v_seed * 6.28318);

  float beat_phase = fract(u_time * 0.52 + v_seed);
  float heartbeat = exp(-pow((beat_phase - 0.08) * 13.0, 2.0)) +
                    0.65 * exp(-pow((beat_phase - 0.25) * 13.0, 2.0));
  float pulse = 0.70 + 0.30 * heartbeat;
  float molten = 0.62 + 0.38 * smoothstep(0.30, 0.70, flow);

  float strength = max(u_magic_strength, 0.0);
  float night = environment_night_amount();
  float kindle = mix(0.85, 1.25, night);

  vec3 sun_color = environment_primary_color() * environment_primary_intensity();

  float ao = clamp(N.y * 0.45 + 0.72, 0.28, 1.0);
  vec3 ambient = environment_ambient_light(N);
  vec3 direct = soi_key_light(N) * 0.85;

  float n_dot_h = max(dot(N, H), 0.0);
  float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);
  float gloss = mix(0.05, 0.42, obsidian) * (1.0 - scorch * 0.5);
  float specular = pow(n_dot_h, mix(24.0, 96.0, obsidian)) * gloss;
  float bronze_spec = pow(n_dot_h, 42.0) * 0.55 * bronze;

  vec3 color = albedo * (ambient + direct) * ao * environment_exposure();
  color += soi_rim_light(N, V) * mix(0.55, 1.0, obsidian);
  color += sun_color * specular * ao;
  color += sun_color * k_bronze * bronze_spec * 1.6 * ao;
  color += environment_sky_color() * fresnel * obsidian * 0.10;

  color = apply_directional_shadow(color, v_world_pos, v_normal);

  vec3 fissure_color =
      mix(k_blood, k_ember, smoothstep(0.40, 0.95, crack_core * molten));
  fissure_color = mix(
      fissure_color, k_ember_hot, smoothstep(0.88, 1.0, crack_core) * base_heat * 0.6);
  vec3 emission = fissure_color * fissure * molten * pulse * 1.15;
  emission += k_abyss * scorch * (1.0 - fissure) * (0.10 + 0.16 * night);
  emission += mix(k_blood, k_ember, 0.55) * spokes * molten * pulse * 1.35;
  emission += mix(k_blood, k_ember, 0.70) * tanit * (0.75 + 0.25 * pulse) * 1.05;
  emission += k_blood * glyphs * pulse * 0.55;
  emission += mix(k_blood, k_ember, coal_field) * hearth * 0.45 * flicker;
  emission += mix(k_ember, k_ember_hot, coals) * coals * flicker * 1.6;
  emission += mix(k_ember, k_ember_hot, 1.0 - smoothstep(0.90, 1.22, lp.y)) *
              max(needle_fire, flames) * flicker * 1.4;
  emission += k_ember * bronze * (1.0 - smoothstep(0.95, 1.40, lp.y)) *
              (1.0 - bull_horns) * 0.04 * flicker;

  float aura =
      fresnel * (1.0 - bronze) * (0.10 + 0.32 * night) * (0.55 + 0.45 * base_heat);
  emission += mix(k_abyss, k_blood, 0.55) * aura * pulse;

  color += emission * strength * kindle;
  color += albedo * ao * local_lighting(v_world_pos, normalize(v_normal));
  color = apply_visibility_world_shading(color, v_world_pos.xz);
  frag_color = vec4(color, 1.0);
}
