#version 330 core

in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_tex_coord;
in float v_billow;
in float v_cloth_depth;
in float v_banner_seed;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform vec3 u_trim_color;
uniform bool u_use_texture;
uniform float u_alpha;
uniform float u_time;
uniform vec3 u_light_direction;
uniform vec3 u_camera_pos;
uniform float u_ambient_strength;

out vec4 frag_color;

float hash(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x),
             mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0)), f.x), f.y);
}

float fbm(vec2 p) {
  float value = 0.0;
  float amplitude = 0.55;
  for (int i = 0; i < 4; ++i) {
    value += noise(p) * amplitude;
    p = p * 2.03 + vec2(5.7, 9.2);
    amplitude *= 0.47;
  }
  return value;
}

float ring_mask(vec2 uv, vec2 center, vec2 radius, float thickness) {
  float d = length((uv - center) / radius);
  return (1.0 - smoothstep(1.0, 1.0 + thickness, d)) *
         smoothstep(1.0 - thickness, 1.0, d);
}

float diamond_mask(vec2 uv, vec2 center, vec2 size) {
  float d = dot(abs((uv - center) / size), vec2(1.0));
  return 1.0 - smoothstep(0.88, 1.04, d);
}

void main() {
  vec2 uv = clamp(v_tex_coord, 0.0, 1.0);

  // Every field standard shares a shallow swallowtail and lightly worn hem.
  // Building banners and rally flags therefore read as one authored family.
  float fork_distance = abs(uv.y - 0.5);
  float tail_limit = mix(0.815, 1.015, smoothstep(0.02, 0.36, fork_distance));
  if (uv.x > tail_limit)
    discard;

  float rectangular_edge = min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));
  float tail_edge_distance = max(tail_limit - uv.x, 0.0);
  float free_hem = min(rectangular_edge, tail_edge_distance);
  float fray_noise = hash(floor(uv * vec2(210.0, 170.0)) + vec2(v_banner_seed * 31.0));
  float fray_zone = 1.0 - smoothstep(0.006, 0.026, free_hem);
  if (uv.x > 0.70 && fray_zone * step(0.84, fray_noise) > 0.5)
    discard;

  vec4 textile = u_use_texture ? texture(u_texture, uv) : vec4(1.0);
  if (textile.a * u_alpha < 0.01)
    discard;

  vec3 N = normalize(v_normal);
  if (!gl_FrontFacing)
    N = -N;
  vec3 L = normalize(u_light_direction);
  vec3 V = normalize(u_camera_pos - v_world_pos);
  vec3 H = normalize(L + V);

  float warp = 0.5 + 0.5 * sin(uv.x * 178.0 + uv.y * 2.0);
  float weft = 0.5 + 0.5 * sin(uv.y * 154.0 - uv.x * 1.5);
  float weave = warp * weft;
  float broad_mottle = fbm(uv * vec2(13.0, 18.0) + vec2(v_banner_seed * 4.7));
  float fiber_variation = noise(uv * vec2(72.0, 61.0) + vec2(v_banner_seed * 13.0));

  vec3 dyed_dark = u_color * vec3(0.58, 0.56, 0.54);
  vec3 dyed_light = min(u_color * vec3(1.13, 1.08, 1.02) + vec3(0.035), vec3(1.0));
  vec3 cloth = mix(dyed_dark, dyed_light,
                   broad_mottle * 0.62 + fiber_variation * 0.23 + weave * 0.15);
  cloth *= mix(0.91, 1.07, weave);
  cloth *= mix(1.0, 0.80, smoothstep(0.05, 0.20, abs(v_billow)));

  float top_fade = (1.0 - smoothstep(0.0, 0.22, uv.y)) *
                   smoothstep(0.18, 0.88, uv.x);
  float trailing_fade = smoothstep(0.70, 1.0, uv.x) *
                        smoothstep(0.30, 0.86, broad_mottle);
  cloth = mix(cloth, cloth + vec3(0.11, 0.085, 0.055),
              top_fade * 0.16 + trailing_fade * 0.10);

  float border_width = 0.066;
  float outer_border = 1.0 - smoothstep(border_width * 0.62, border_width, free_hem);
  float mast_binding = 1.0 - smoothstep(0.065, 0.115, abs(uv.x - 0.10));
  float center_ring = ring_mask(uv, vec2(0.49, 0.51), vec2(0.165, 0.205), 0.15);
  float center_diamond = diamond_mask(uv, vec2(0.49, 0.51), vec2(0.052, 0.086));
  float embroidery = clamp(max(outer_border,
                               max(mast_binding * 0.72,
                                   max(center_ring, center_diamond * 0.90))),
                           0.0, 1.0);

  float stitch = 0.5 + 0.5 * sin((uv.x + uv.y) * 245.0);
  vec3 trim_shadow = u_trim_color * vec3(0.48, 0.43, 0.34);
  vec3 trim_light = min(u_trim_color * vec3(1.18, 1.12, 0.96) + vec3(0.025),
                        vec3(1.0));
  vec3 trim = mix(trim_shadow, trim_light, 0.38 + stitch * 0.42 + weave * 0.20);
  vec3 albedo = mix(cloth, trim, embroidery * 0.96);

  float lower_dirt = smoothstep(0.70, 1.0, uv.y) *
                     smoothstep(0.44, 0.82,
                                fbm(uv * vec2(22.0, 11.0) + vec2(9.0, v_banner_seed)));
  albedo = mix(albedo, albedo * vec3(0.55, 0.50, 0.43), lower_dirt * 0.24);

  float ndotl = dot(N, L);
  float wrap = clamp((ndotl + 0.34) / 1.34, 0.0, 1.0);
  float hemi = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
  vec3 sky = vec3(0.50, 0.59, 0.70);
  vec3 sun = vec3(1.00, 0.88, 0.70);
  float ambient = clamp(u_ambient_strength, 0.12, 0.55);
  vec3 illumination = sky * (ambient * 0.72 + hemi * 0.12) + sun * wrap * 0.72;

  float pinned_ao = mix(0.76, 1.0, smoothstep(0.0, 0.20, uv.x));
  float fold_ao = mix(0.72, 1.0, smoothstep(0.015, 0.15, abs(v_billow)));
  float backscatter = max(dot(-N, L), 0.0) * (1.0 - embroidery * 0.28);
  float cloth_sheen = pow(max(dot(N, H), 0.0), 13.0) * 0.065;
  float thread_sheen = pow(max(dot(N, H), 0.0), 38.0) * embroidery * 0.34;
  float grazing = pow(1.0 - max(dot(N, V), 0.0), 4.0) * 0.055;

  vec3 color = albedo * illumination * pinned_ao * fold_ao;
  color += cloth * sun * backscatter * 0.16;
  color += sky * cloth_sheen;
  color += trim_light * sun * thread_sheen;
  color += sky * grazing;

  frag_color = vec4(clamp(color * textile.rgb, 0.0, 1.0),
                    textile.a * u_alpha);
}
