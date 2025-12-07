#version 330 core

// ============================================================================
// CATAPULT FRAGMENT SHADER
// Wood and metal materials for siege equipment
// ============================================================================

in vec3 v_normal;
in vec2 v_texCoord;
in vec3 v_worldPos;
in float v_materialRegion;

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

// Wood grain pattern
float wood_grain(vec2 p) {
  float grain = sin(p.y * 30.0 + noise(p * 5.0) * 3.0) * 0.5 + 0.5;
  float fine_grain = noise(p * 50.0) * 0.2;
  return grain * 0.15 + fine_grain;
}

// Metal surface pattern
float metal_surface(vec2 p) {
  float scratches = noise(p * 80.0) * 0.1;
  float polish = noise(p * 20.0) * 0.05;
  return scratches + polish;
}

void main() {
  vec3 color = u_color;
  if (u_useTexture) {
    color *= texture(u_texture, v_texCoord).rgb;
  }

  vec3 normal = normalize(v_normal);
  vec2 uv = v_worldPos.xz * 4.0;
  float avg_color = (color.r + color.g + color.b) / 3.0;

  // Detect material type from color
  bool is_wood =
      (color.r > color.b * 1.2 && avg_color > 0.25 && avg_color < 0.60);
  bool is_metal =
      (avg_color > 0.30 && avg_color < 0.55 && abs(color.r - color.g) < 0.1 &&
       abs(color.g - color.b) < 0.1);
  bool is_rope = (avg_color > 0.40 && avg_color < 0.65 && color.r > color.b);
  bool is_leather =
      (avg_color > 0.20 && avg_color < 0.45 && color.r > color.b * 1.3);

  // === WOOD MATERIALS ===
  if (is_wood) {
    float grain = wood_grain(v_worldPos.xz);
    float knots = step(0.92, noise(uv * 3.0)) * 0.15;

    // Wood has subtle sheen along grain
    float view_angle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.3))));
    float wood_sheen = pow(1.0 - view_angle, 6.0) * 0.08;

    color *= 1.0 + grain - knots;
    color += vec3(wood_sheen);
  }
  // === METAL MATERIALS ===
  else if (is_metal) {
    float surface = metal_surface(v_worldPos.xz);

    // Metal has stronger specular
    float view_angle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.4))));
    float metal_sheen = pow(1.0 - view_angle, 4.0) * 0.18;

    // Slight rust/patina on exposed metal
    float patina = noise(uv * 8.0) * 0.08;

    color *= 1.0 + surface - patina;
    color += vec3(metal_sheen);
  }
  // === ROPE MATERIALS ===
  else if (is_rope) {
    // Twisted fiber pattern
    float twist = sin(v_worldPos.y * 40.0 + v_worldPos.x * 10.0) * 0.08;
    float fiber = noise(uv * 60.0) * 0.12;

    color *= 1.0 + twist + fiber - 0.05;
  }
  // === LEATHER MATERIALS ===
  else if (is_leather) {
    float grain = noise(uv * 20.0) * 0.15;
    float crease = noise(uv * 8.0) * 0.10;

    float view_angle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.3))));
    float leather_sheen = pow(1.0 - view_angle, 5.0) * 0.10;

    color *= 1.0 + grain - crease;
    color += vec3(leather_sheen);
  }
  // === DEFAULT ===
  else {
    float detail = noise(uv * 15.0) * 0.08;
    color *= 1.0 + detail - 0.04;
  }

  color = clamp(color, 0.0, 1.0);

  // Lighting
  vec3 light_dir = normalize(vec3(1.0, 1.2, 0.8));
  float n_dot_l = dot(normal, light_dir);
  float wrap = 0.35;
  float diff = max(n_dot_l * (1.0 - wrap) + wrap, 0.30);

  color *= diff;
  FragColor = vec4(color, u_alpha);
}
