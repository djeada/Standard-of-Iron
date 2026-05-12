#version 330 core

in vec3 v_normal;
in vec2 v_tex_coord;
in vec3 v_world_pos;
flat in vec3 v_instance_color;
flat in float v_instance_alpha;
in float v_material_region;

uniform sampler2_d u_texture;
uniform bool u_use_texture;

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

float wood_grain(vec2 p) {
  float grain = sin(p.y * 30.0 + noise(p * 5.0) * 3.0) * 0.5 + 0.5;
  float fine_grain = noise(p * 50.0) * 0.2;
  return grain * 0.15 + fine_grain;
}

float metal_surface(vec2 p) {
  float scratches = noise(p * 80.0) * 0.1;
  float polish = noise(p * 20.0) * 0.05;
  return scratches + polish;
}

void main() {
  vec3 color = v_instance_color;
  if (u_use_texture) {
    color *= texture(u_texture, v_tex_coord).rgb;
  }

  vec3 normal = normalize(v_normal);
  vec2 uv = v_world_pos.xz * 4.0;
  float avg_color = (color.r + color.g + color.b) / 3.0;

  bool is_wood =
      (color.r > color.b * 1.2 && avg_color > 0.25 && avg_color < 0.60);
  bool is_metal =
      (avg_color > 0.30 && avg_color < 0.55 && abs(color.r - color.g) < 0.1 &&
       abs(color.g - color.b) < 0.1);
  bool is_rope = (avg_color > 0.40 && avg_color < 0.65 && color.r > color.b);
  bool is_leather =
      (avg_color > 0.20 && avg_color < 0.45 && color.r > color.b * 1.3);

  if (is_wood) {
    float grain = wood_grain(v_world_pos.xz);
    float knots = step(0.92, noise(uv * 3.0)) * 0.15;

    float view_angle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.3))));
    float wood_sheen = pow(1.0 - view_angle, 6.0) * 0.08;

    color *= 1.0 + grain - knots;
    color += vec3(wood_sheen);
  }

  else if (is_metal) {
    float surface = metal_surface(v_world_pos.xz);

    float view_angle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.4))));
    float metal_sheen = pow(1.0 - view_angle, 4.0) * 0.18;

    float patina = noise(uv * 8.0) * 0.08;

    color *= 1.0 + surface - patina;
    color += vec3(metal_sheen);
  }

  else if (is_rope) {

    float twist = sin(v_world_pos.y * 40.0 + v_world_pos.x * 10.0) * 0.08;
    float fiber = noise(uv * 60.0) * 0.12;

    color *= 1.0 + twist + fiber - 0.05;
  }

  else if (is_leather) {
    float grain = noise(uv * 20.0) * 0.15;
    float crease = noise(uv * 8.0) * 0.10;

    float view_angle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.3))));
    float leather_sheen = pow(1.0 - view_angle, 5.0) * 0.10;

    color *= 1.0 + grain - crease;
    color += vec3(leather_sheen);
  }

  else {
    float detail = noise(uv * 15.0) * 0.08;
    color *= 1.0 + detail - 0.04;
  }

  color = clamp(color, 0.0, 1.0);

  vec3 light_dir = normalize(vec3(0.65, 0.50, 0.40));
  float n_dot_l = dot(normal, light_dir);
  float wrap = 0.28;
  float diff_raw = n_dot_l * (1.0 - wrap) + wrap;
  float diff = max(diff_raw, 0.18);

  vec3 sun_color = vec3(1.08, 0.92, 0.74);
  vec3 sky_color = vec3(0.72, 0.80, 1.00);
  float lit_t = clamp((diff_raw + 1.0) / 2.5, 0.0, 1.0);
  vec3 light_tint = mix(sky_color * 0.34, sun_color, lit_t);

  color *= diff * light_tint;
  frag_color = vec4(color, v_instance_alpha);
}
