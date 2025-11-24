#version 330 core

in vec3 v_normal;
in vec2 v_texCoord;
in vec3 v_worldPos;
in float v_armorLayer;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_useTexture;
uniform float u_alpha;
uniform int u_materialId;

out vec4 FragColor;

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

float cloth_weave(vec2 p) {
  float warp_thread = sin(p.x * 70.0);
  float weft_thread = sin(p.y * 68.0);
  return warp_thread * weft_thread * 0.06;
}

float phoenician_linen(vec2 p) {
  float weave = cloth_weave(p);
  float fine_thread = noise(p * 88.0) * 0.08;
  float tyrian = noise(p * 12.0) * 0.05;
  return weave + fine_thread + tyrian;
}

void main() {
  vec3 color = u_color;
  if (u_useTexture) {
    color *= texture(u_texture, v_texCoord).rgb;
  }

  vec3 normal = normalize(v_normal);
  vec2 uv = v_worldPos.xz * 4.5;
  float avg_color = (color.r + color.g + color.b) / 3.0;

  // Material ID: 0=body/skin, 1=armor, 2=helmet, 3=weapon, 4=shield
  bool is_armor = (u_materialId == 1);
  bool is_helmet = (u_materialId == 2);
  bool is_weapon = (u_materialId == 3);

  // Use material IDs exclusively (no fallbacks)

  bool is_light = (avg_color > 0.74);
  bool is_purple = (color.b > color.g * 1.15 && color.b > color.r * 1.08);

  // LIGHT LINEN ROBES (Phoenician healer)
  if (is_light || avg_color > 0.70) {
    float linen = phoenician_linen(v_worldPos.xz);
    float mediterranean_folds = noise(uv * 8.5) * 0.13;

    float view_angle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.2))));
    float linen_sheen = pow(1.0 - view_angle, 9.0) * 0.13;

    color *= 1.0 + linen + mediterranean_folds - 0.02;
    color += vec3(linen_sheen);
  }
  // PURPLE CAPE/TRIM (Tyrian purple - rare healer dye)
  else if (is_purple) {
    float weave = cloth_weave(v_worldPos.xz);
    float tyrian_richness = noise(uv * 6.0) * 0.15;
    float luxury_shimmer = noise(uv * 35.0) * 0.04;

    float view_angle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.3))));
    float silk_sheen = pow(1.0 - view_angle, 6.0) * 0.14;

    color *= 1.0 + weave + tyrian_richness + luxury_shimmer - 0.03;
    color += vec3(silk_sheen);
  }
  // LEATHER ELEMENTS (sandals, belt)
  else if (avg_color > 0.30 && avg_color <= 0.56) {
    float leather_grain = noise(uv * 14.0) * 0.14;
    float phoenician_craft = noise(uv * 24.0) * 0.06;

    float view_angle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.4))));
    float leather_sheen = pow(1.0 - view_angle, 6.0) * 0.10;

    color *= 1.0 + leather_grain + phoenician_craft - 0.04;
    color += vec3(leather_sheen);
  } else {
    float detail = noise(uv * 10.0) * 0.10;
    color *= 1.0 + detail - 0.06;
  }

  color = clamp(color, 0.0, 1.0);

  vec3 light_dir = normalize(vec3(1.0, 1.2, 1.0));
  float n_dot_l = dot(normal, light_dir);
  float wrap_amount = 0.46;
  float diff = max(n_dot_l * (1.0 - wrap_amount) + wrap_amount, 0.28);

  color *= diff;
  FragColor = vec4(color, u_alpha);
}
