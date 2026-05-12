#version 330 core

in vec3 v_normal;
in vec2 v_tex_coord;
in vec3 v_world_pos;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_use_texture;
uniform float u_alpha;
uniform int u_material_id;
uniform vec3 u_light_dir;
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
  float a = hash(i);
  float b = hash(i + vec2(1.0, 0.0));
  float c = hash(i + vec2(0.0, 1.0));
  float d = hash(i + vec2(1.0, 1.0));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float wood_grain(vec2 p, float y) {
  float grain = sin(y * 22.0 + noise(p * 4.5) * 3.5) * 0.5 + 0.5;
  float fine = noise(p * 48.0) * 0.10;
  float knot = step(0.93, noise(p * 2.2)) * 0.16;
  return grain * 0.13 + fine - knot;
}

vec3 procedural_material_variation(vec3 base_color, vec3 world_pos, vec3 normal) {
  vec2 uv = world_pos.xz * 4.0;

  float avg_color = (base_color.r + base_color.g + base_color.b) / 3.0;

  bool b_is_wood = false, b_is_metal = false, b_is_cloth = false;
  if (u_material_id == 2) {
    b_is_wood = true;
  } else if (u_material_id == 1) {
    b_is_metal = true;
  } else if (u_material_id == 3) {
    b_is_cloth = true;
  } else if (u_material_id != 4) {

    b_is_wood =
        (base_color.r < base_color.g * 2.5 && base_color.r > base_color.b * 1.45 &&
         avg_color > 0.18 && avg_color < 0.50);
    b_is_metal = (!b_is_wood && avg_color < 0.40);
    b_is_cloth = (!b_is_wood && !b_is_metal && avg_color > 0.65);
  }

  vec3 variation = base_color;

  if (b_is_wood) {
    float wood_noise = wood_grain(uv, world_pos.y);
    float view_angle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.3))));
    float sheen = pow(1.0 - view_angle, 4.0) * 0.06;
    variation = base_color * (1.0 + wood_noise) + vec3(sheen);
  } else if (b_is_metal) {
    float metal_noise = noise(uv * 9.0) * 0.018;
    float view_angle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
    float fresnel = pow(1.0 - view_angle, 2.0) * 0.10;
    variation = base_color + vec3(metal_noise + fresnel);
  } else if (b_is_cloth) {
    float weave_x = sin(world_pos.x * 55.0);
    float weave_z = sin(world_pos.z * 55.0);
    float weave_pattern = weave_x * weave_z * 0.025;
    float cloth_noise = noise(uv * 2.5) * 0.10 - 0.05;

    float view_angle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
    float sheen = pow(1.0 - view_angle, 3.0) * 0.15;

    variation = base_color * (1.0 + cloth_noise + weave_pattern) + vec3(sheen);
  } else {
    float leather_noise = noise(uv * 5.5);
    float blotches = noise(uv * 1.8) * 0.12 - 0.06;
    variation = base_color * (1.0 + leather_noise * 0.14 - 0.07 + blotches);
  }

  return clamp(variation, 0.0, 1.0);
}

void main() {
  vec3 color = u_color;
  if (u_use_texture) {
    color *= texture(u_texture, v_tex_coord).rgb;
  }

  vec3 normal = normalize(v_normal);
  color = procedural_material_variation(color, v_world_pos, normal);

  if (u_material_id >= 10) {
    float soot_amt = float(u_material_id - 9) * 0.45;
    vec2 soot_uv = v_world_pos.xz * 3.5;
    float soot_patch = noise(soot_uv) * 0.6 + noise(soot_uv * 4.1) * 0.4;
    float soot_mask = smoothstep(0.42, 0.65, soot_patch) * soot_amt;
    vec3 char_color = mix(color * 0.25, vec3(0.08, 0.07, 0.06), 0.5);
    color = mix(color, char_color, clamp(soot_mask, 0.0, 0.85));
  }

  vec3 light_dir = length(u_light_dir) > 0.001
                      ? u_light_dir
                      : normalize(vec3(0.65, 0.50, 0.40));
  float avg_color = (u_color.r + u_color.g + u_color.b) / 3.0;
  float wrap_amount = avg_color > 0.65 ? 0.52 : (avg_color > 0.40 ? 0.20 : 0.05);

  float n_dot_l = dot(normal, light_dir);
  float diff_raw = n_dot_l * (1.0 - wrap_amount) + wrap_amount;
  float ambient = u_ambient_strength > 0.001 ? u_ambient_strength : 0.18;
  float diff = max(diff_raw, ambient);

  vec3 sun_color = vec3(1.08, 0.92, 0.74);
  vec3 sky_color = vec3(0.72, 0.80, 1.00);
  float lit_t = clamp((diff_raw + 1.0) / 2.5, 0.0, 1.0);
  vec3 light_tint = mix(sky_color * (ambient * 1.1), sun_color, lit_t);

  color *= diff * light_tint;
  frag_color = vec4(color, u_alpha);
}
