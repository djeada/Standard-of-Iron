#version 330 core

in vec3 v_normal;
in vec2 v_tex_coord;
in float v_wave_offset;
in float v_cloth_depth;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform vec3 u_trim_color;
uniform bool u_use_texture;
uniform float u_alpha;
uniform float u_time;

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
  float a = hash(i);
  float b = hash(i + vec2(1.0, 0.0));
  float c = hash(i + vec2(0.0, 1.0));
  float d = hash(i + vec2(1.0, 1.0));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
  float value = 0.0;
  float amplitude = 0.5;
  for (int i = 0; i < 3; i++) {
    value += amplitude * noise(p);
    p *= 2.0;
    amplitude *= 0.5;
  }
  return value;
}

float cloth_weave(vec2 uv) {
  float warp = sin(uv.x * 130.0) * 0.5 + 0.5;
  float weft = sin(uv.y * 110.0) * 0.5 + 0.5;
  return (warp * weft - 0.25) * 0.12;
}

float ring_mask(vec2 uv, vec2 center, vec2 radius, float thickness) {
  vec2 p = (uv - center) / radius;
  float d = length(p);
  return (1.0 - smoothstep(1.0, 1.0 + thickness, d)) *
         smoothstep(1.0 - thickness, 1.0, d);
}

float diamond_mask(vec2 uv, vec2 center, vec2 size) {
  vec2 p = abs((uv - center) / size);
  float d = p.x + p.y;
  return 1.0 - smoothstep(0.9, 1.05, d);
}

void main() {
  vec2 uv = v_tex_coord;
  vec3 color = u_color;
  if (u_use_texture) {
    color *= texture(u_texture, uv).rgb;
  }

  vec3 normal = normalize(v_normal);
  if (!gl_FrontFacing) {
    normal = -normal;
  }

  float weave = cloth_weave(uv);

  float fold_shadow = clamp(1.0 - abs(v_wave_offset) * 2.2, 0.76, 1.0);
  float mast_shadow = 1.0 - smoothstep(0.0, 0.22, uv.x) * 0.16;
  float hem_tone = mix(0.94, 1.04, uv.y);
  float fabric_noise =
      fbm(uv * vec2(16.0, 22.0) + vec2(0.0, u_time * 0.03)) * 0.08 - 0.04;

  color *= (1.0 + fabric_noise + weave) * fold_shadow * mast_shadow * hem_tone;

  float border_width = 0.08;
  float edge_dist = min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));
  float border_mask =
      1.0 - smoothstep(border_width * 0.65, border_width, edge_dist);
  float mast_band = 1.0 - smoothstep(0.08, 0.14, abs(uv.x - 0.14));
  float roundel = ring_mask(uv, vec2(0.52, 0.54), vec2(0.16, 0.20), 0.16);
  float diamond = diamond_mask(uv, vec2(0.52, 0.54), vec2(0.055, 0.09));
  float motif =
      max(border_mask, max(mast_band * 0.65, max(roundel, diamond * 0.85)));
  color = mix(color, u_trim_color, motif * 0.92);

  vec3 light_dir = normalize(vec3(0.55, 0.80, 0.40));
  float n_dot_l = dot(normal, light_dir);

  float wrap_amount = 0.55;
  float diff = max(n_dot_l * (1.0 - wrap_amount) + wrap_amount, 0.22);

  float ao = 1.0 - v_cloth_depth * 0.12;
  float backscatter =
      max(dot(-normal, light_dir), 0.0) * 0.12 * (1.0 - v_cloth_depth * 0.35);
  vec3 view_dir = normalize(vec3(0.0, 0.7, 0.7));
  vec3 half_dir = normalize(light_dir + view_dir);
  float sheen = pow(max(dot(normal, half_dir), 0.0), 18.0) * 0.08;

  color *= diff * ao;
  color += u_color * (0.24 + backscatter);
  color += u_trim_color * sheen;
  color = mix(color, color * 0.94, v_cloth_depth * 0.28);

  frag_color = vec4(clamp(color, 0.0, 1.0), u_alpha);
}
