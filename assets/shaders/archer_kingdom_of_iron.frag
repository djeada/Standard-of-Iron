#version 330 core

in vec3 v_normal;
in vec2 v_texCoord;
in vec3 v_worldPos;
in float v_armorLayer; // Armor layer from vertex shader

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_useTexture;
uniform float u_alpha;

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

// Quilted gambeson texture (padded cloth armor)
float gambesonQuilt(vec2 p) {
  // Diamond quilting pattern
  vec2 grid = p * 12.0;
  float diagA = sin((grid.x + grid.y) * 3.14159);
  float diagB = sin((grid.x - grid.y) * 3.14159);
  float quilt = diagA * diagB * 0.12;

  // Stuffing bumps
  float padding = noise(p * 18.0) * 0.10;

  return quilt + padding;
}

// Riveted mail (lighter European chainmail)
float rivetedMail(vec2 p) {
  vec2 grid = fract(p * 28.0) - 0.5;
  float ring = length(grid);
  float ringPattern =
      smoothstep(0.40, 0.35, ring) - smoothstep(0.30, 0.25, ring);

  // Offset rows
  vec2 offsetGrid = fract(p * 28.0 + vec2(0.5, 0.0)) - 0.5;
  float offsetRing = length(offsetGrid);
  float offsetPattern =
      smoothstep(0.40, 0.35, offsetRing) - smoothstep(0.30, 0.25, offsetRing);

  // Rivets on rings (more visible than Roman mail)
  float rivet = noise(p * 30.0) * 0.06;

  return (ringPattern + offsetPattern) * 0.16 + rivet;
}

// Wool/linen cloth texture
float clothWeave(vec2 p) {
  float warpThread = sin(p.x * 70.0);
  float weftThread = sin(p.y * 68.0);
  return warpThread * weftThread * 0.06;
}

void main() {
  vec3 color = u_color;
  if (u_useTexture) {
    color *= texture(u_texture, v_texCoord).rgb;
  }

  vec3 normal = normalize(v_normal);
  vec2 uv = v_worldPos.xz * 4.5;
  float avgColor = (color.r + color.g + color.b) / 3.0;

  // Detect materials by color and layer
  bool isSteel = (avgColor > 0.55 && avgColor <= 0.75);
  bool isGreen = (color.g > color.r * 1.15 && color.g > color.b * 1.05);
  bool isBrown =
      (color.r > color.g * 1.05 && color.r > color.b * 1.12 && avgColor < 0.60);
  bool isHelmet = (v_armorLayer == 0.0);
  bool isTorsoArmor = (v_armorLayer == 1.0);

  // === KINGDOM/MEDIEVAL ARCHER (ENGLISH LONGBOWMAN STYLE) ===

  // STEEL KETTLE HELMET or CHAPEL DE FER
  if (isSteel && isHelmet) {
    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.3))));
    float steelSheen = pow(viewAngle, 8.0) * 0.28;
    float steelFresnel = pow(1.0 - viewAngle, 2.5) * 0.22;

    // Hammer marks and imperfections
    float hammerMarks = noise(uv * 22.0) * 0.04;
    float scratches = noise(uv * 35.0) * 0.03;

    color += vec3(steelSheen + steelFresnel);
    color += vec3(hammerMarks + scratches);
  }
  // PADDED GAMBESON (quilted cloth armor) - PRIMARY DEFENSE
  else if (isTorsoArmor && (isBrown || avgColor > 0.45 && avgColor < 0.65)) {
    // Thick quilted padding
    float quilt = gambesonQuilt(v_worldPos.xz);

    // Wool/linen texture
    float weave = clothWeave(v_worldPos.xz);

    // Soft matte finish (not shiny)
    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.2))));
    float clothSheen = pow(1.0 - viewAngle, 12.0) * 0.06;

    // Natural wear and dirt
    float wear = noise(uv * 6.0) * 0.12 - 0.06;

    color *= 1.0 + quilt + weave - 0.04 + wear;
    color += vec3(clothSheen);
  }
  // LIGHT MAIL SHIRT (optional, over gambeson)
  else if (isTorsoArmor && isSteel) {
    float mail = rivetedMail(v_worldPos.xz);

    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.4))));
    float mailSheen = pow(viewAngle, 6.0) * 0.18;

    // Light rust
    float rust = noise(uv * 11.0) * 0.06;

    color += vec3(mail + mailSheen);
    color -= vec3(rust * 0.3);
  }
  // GREEN TUNIC/HOOD (Lincoln green - iconic archer color)
  else if (isGreen) {
    float weave = clothWeave(v_worldPos.xz);
    float woolFuzz = noise(uv * 22.0) * 0.09;

    // Natural dye variations
    float dyeVariation = noise(uv * 5.0) * 0.10;

    // Soft cloth sheen
    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.3))));
    float clothSheen = pow(1.0 - viewAngle, 10.0) * 0.07;

    color *= 1.0 + weave + woolFuzz - 0.03 + dyeVariation - 0.05;
    color += vec3(clothSheen);
  }
  // LEATHER ELEMENTS (belt, bracers, boots)
  else if (avgColor > 0.30 && avgColor <= 0.55) {
    float leatherGrain = noise(uv * 14.0) * 0.16;
    float tooling = noise(uv * 20.0) * 0.06;
    float wear = noise(uv * 4.0) * 0.08 - 0.04;

    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.4))));
    float leatherSheen = pow(1.0 - viewAngle, 5.0) * 0.12;

    color *= 1.0 + leatherGrain + tooling - 0.06 + wear;
    color += vec3(leatherSheen);
  }
  // DARK ELEMENTS (boots, belts, straps)
  else {
    float darkDetail = noise(uv * 10.0) * 0.12;
    float wear = noise(uv * 3.0) * 0.06;

    color *= 1.0 + darkDetail - 0.08 - wear;
  }

  color = clamp(color, 0.0, 1.0);

  // Lighting - softer for cloth/padding, harder for metal
  vec3 lightDir = normalize(vec3(1.0, 1.2, 1.0));
  float nDotL = dot(normal, lightDir);

  // Cloth has more wrap-around lighting
  float wrapAmount = (isSteel && isHelmet) ? 0.15 : 0.42;
  float diff = max(nDotL * (1.0 - wrapAmount) + wrapAmount, 0.24);

  color *= diff;
  FragColor = vec4(color, u_alpha);
}
