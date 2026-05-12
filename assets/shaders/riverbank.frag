#version 330 core
out vec4 frag_color;

in vec2 tex_coord;
in vec3 world_pos;
in vec3 v_normal;

uniform float time;
uniform sampler2D u_visibility_tex;
uniform vec2 u_visibility_size;
uniform float u_visibility_tile_size;
uniform float u_explored_alpha;
uniform int u_has_visibility;
uniform float u_segment_visibility;

float saturate(float x) { return clamp(x, 0.0, 1.0); }
vec2 saturate(vec2 v) { return clamp(v, vec2(0.0), vec2(1.0)); }
vec3 saturate(vec3 v) { return clamp(v, vec3(0.0), vec3(1.0)); }

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
  float v = 0.0, a = 0.5, f = 1.0;
  for (int i = 0; i < 5; i++) {
    v += a * noise(p * f);
    f *= 2.0;
    a *= 0.5;
  }
  return v;
}
vec2 warp(vec2 uv) {
  vec2 w1 = vec2(fbm(uv * 0.7 + time * 0.06), fbm(uv * 0.7 - time * 0.05));
  vec2 w2 = vec2(fbm(uv * 1.1 - time * 0.03), fbm(uv * 0.9 + time * 0.04));
  return uv + 0.22 * w1 + 0.12 * w2;
}

void main() {

  vec2 uv = warp(world_pos.xz * 0.45);

  float visibility_factor = 1.0;
  if (u_has_visibility == 1 && u_visibility_size.x > 0.0 &&
      u_visibility_size.y > 0.0) {
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

  vec3 wet_soil = vec3(0.20, 0.17, 0.14);
  vec3 damp_soil = vec3(0.32, 0.27, 0.22);
  vec3 dry_soil = vec3(0.46, 0.40, 0.33);
  vec3 grass_tint = vec3(0.30, 0.50, 0.25);

  float base_wet = smoothstep(0.30, 0.04, tex_coord.x);

  float edge_jitter = (fbm(uv * 2.5) - 0.5) * 0.12 +
                     (fbm(vec2(tex_coord.y * 3.0, 0.0)) - 0.5) * 0.10;
  base_wet = saturate(base_wet + edge_jitter);

  float slope = 1.0 - saturate(normalize(v_normal).y);
  float wetness = saturate(base_wet - slope * 0.45);

  float contact =
      smoothstep(0.10, 0.95, wetness) * smoothstep(0.04, 0.00, tex_coord.x);
  contact *= 0.6 + 0.4 * fbm(uv * 4.0 + time * 0.2);

  float macro = fbm(uv * 0.8);
  float streaks =
      fbm(vec2(tex_coord.y * 6.0 + macro * 0.7, tex_coord.x * 0.6 - time * 0.03));
  streaks = pow(saturate(streaks), 3.0);

  float grit = noise(uv * 18.0);
  float pebbles = smoothstep(0.82, 0.97, noise(uv * 9.0 + 17.3));

  vec3 soil_wet = mix(wet_soil, damp_soil, saturate(macro * 0.5 + 0.25));
  vec3 soil_dry = mix(damp_soil, dry_soil, saturate(macro * 0.5 + 0.25));
  vec3 base = mix(soil_wet, soil_dry, 1.0 - wetness);

  float grass_mask = smoothstep(0.50, 0.92, 1.0 - wetness) *
                    smoothstep(0.25, 0.75, fbm(uv * 1.2 + 5.1));
  base = mix(base, mix(base, grass_tint, 0.6), grass_mask);

  base = mix(base, base * vec3(0.92, 0.96, 1.02), contact * 0.35);

  base *= (0.92 + 0.16 * grit);
  base = mix(base, base * 0.82 + vec3(0.05),
             pebbles * (0.10 + 0.35 * (1.0 - wetness)));

  vec3 L = normalize(vec3(0.3, 0.8, 0.4));
  vec3 V = normalize(vec3(0.0, 1.0, 0.5));
  vec3 N = normalize(v_normal);

  float NdotL = max(dot(N, L), 0.0);

  float darken = mix(1.0, 0.94, wetness);

  vec3 ambient = vec3(0.42, 0.44, 0.46);
  vec3 color = base * darken * (ambient + NdotL * vec3(0.66, 0.64, 0.60));

  vec3 H = normalize(L + V);
  float shininess = mix(10.0, 38.0, saturate(wetness * 1.2));
  float spec = pow(max(dot(N, H), 0.0), shininess);
  spec *= (0.10 + 0.50 * contact);
  color += spec * vec3(0.30, 0.33, 0.36);

  color *= (1.0 - streaks * 0.07 * (0.3 + 0.7 * wetness));

  color *= visibility_factor;

  frag_color = vec4(saturate(color), 1.0);
}
