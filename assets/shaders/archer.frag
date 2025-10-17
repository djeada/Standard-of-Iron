#version 330 core

in vec3 v_normal;
in vec2 v_texCoord;
in vec3 v_worldPos;

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

// Roman chainmail (lorica hamata) ring pattern
float chainmailRings(vec2 p) {
  vec2 grid = fract(p * 32.0) - 0.5;
  float ring = length(grid);
  float ringPattern =
      smoothstep(0.38, 0.32, ring) - smoothstep(0.28, 0.22, ring);

  // Offset rows for interlocking
  vec2 offsetGrid = fract(p * 32.0 + vec2(0.5, 0.0)) - 0.5;
  float offsetRing = length(offsetGrid);
  float offsetPattern =
      smoothstep(0.38, 0.32, offsetRing) - smoothstep(0.28, 0.22, offsetRing);

  return (ringPattern + offsetPattern) * 0.14;
}

// Leather pteruges strips (hanging skirt/shoulder guards)
float pterugesStrips(vec2 p, float y) {
  // Vertical leather strips
  float stripX = fract(p.x * 9.0);
  float strip = smoothstep(0.15, 0.20, stripX) - smoothstep(0.80, 0.85, stripX);

  // Add leather texture to strips
  float leatherTex = noise(p * 18.0) * 0.35;

  // Strips hang and curve
  float hang = smoothstep(0.65, 0.45, y);

  return strip * leatherTex * hang;
}

void main() {
  vec3 color = u_color;
  if (u_useTexture) {
    color *= texture(u_texture, v_texCoord).rgb;
  }

  vec3 normal = normalize(v_normal);
  vec2 uv = v_worldPos.xz * 4.5;
  float avgColor = (color.r + color.g + color.b) / 3.0;

  // Detect bronze vs steel by color warmth
  bool isBronze =
      (color.r > color.g * 1.08 && color.r > color.b * 1.15 && avgColor > 0.50);
  bool isRedCape = (color.r > color.g * 1.3 && color.r > color.b * 1.4);

  // === ROMAN ARCHER (SAGITTARIUS) MATERIALS ===

  // BRONZE GALEA HELMET & PHALERAE (warm golden metal)
  if (isBronze) {
    // Ancient bronze patina and wear
    float bronzePatina = noise(uv * 8.0) * 0.12;
    float verdigris = noise(uv * 15.0) * 0.08; // Green oxidation

    // Bronze is less reflective than polished steel
    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
    float bronzeSheen = pow(viewAngle, 7.0) * 0.25;
    float bronzeFresnel = pow(1.0 - viewAngle, 2.2) * 0.18;

    // Hammer marks from forging
    float hammerMarks = noise(uv * 25.0) * 0.035;

    color += vec3(bronzeSheen + bronzeFresnel);
    color -= vec3(bronzePatina * 0.4 + verdigris * 0.3);
    color += vec3(hammerMarks * 0.5);
  }
  // STEEL CHAINMAIL (lorica hamata - grey-blue tint)
  else if (avgColor > 0.40 && avgColor <= 0.60 && !isRedCape) {
    // Interlocked iron rings
    float rings = chainmailRings(v_worldPos.xz);

    // Chainmail has dull metallic sheen
    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
    float chainSheen = pow(viewAngle, 5.0) * 0.16;

    // Iron rust spots
    float rust = noise(uv * 10.0) * 0.08;

    color += vec3(rings + chainSheen);
    color -= vec3(rust * 0.4);              // Darken with age
    color *= 1.0 - noise(uv * 18.0) * 0.06; // Shadow between rings
  }
  // RED SAGUM CAPE (bright red woolen cloak)
  else if (isRedCape) {
    // Thick woolen weave
    float weaveX = sin(v_worldPos.x * 55.0);
    float weaveZ = sin(v_worldPos.z * 55.0);
    float weave = weaveX * weaveZ * 0.045;

    // Wool texture (fuzzy)
    float woolFuzz = noise(uv * 20.0) * 0.10;

    // Fabric folds and draping
    float folds = noise(uv * 6.0) * 0.12 - 0.06;

    // Soft fabric sheen
    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
    float capeSheen = pow(1.0 - viewAngle, 8.0) * 0.08;

    color *= 1.0 + woolFuzz - 0.05 + folds;
    color += vec3(weave + capeSheen);
  }
  // LEATHER PTERUGES & ARMOR STRIPS (tan/brown leather strips)
  else if (avgColor > 0.35) {
    // Thick leather with visible grain
    float leatherGrain = noise(uv * 10.0) * 0.16;
    float leatherPores = noise(uv * 22.0) * 0.08;

    // Pteruges strip pattern
    float strips = pterugesStrips(v_worldPos.xz, v_worldPos.y);

    // Worn leather edges
    float wear = noise(uv * 4.0) * 0.10 - 0.05;

    // Leather has subtle sheen
    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
    float leatherSheen = pow(1.0 - viewAngle, 4.5) * 0.10;

    color *= 1.0 + leatherGrain + leatherPores - 0.08 + wear;
    color += vec3(strips * 0.15 + leatherSheen);
  }
  // DARK ELEMENTS (cingulum belt, straps, manicae)
  else {
    float leatherDetail = noise(uv * 8.0) * 0.14;
    float tooling = noise(uv * 16.0) * 0.06; // Decorative tooling
    float darkening = noise(uv * 2.5) * 0.08;

    color *= 1.0 + leatherDetail - 0.07 + tooling - darkening;
  }

  color = clamp(color, 0.0, 1.0);

  // Lighting model - soft wrap for leather/fabric, harder for metal
  vec3 lightDir = normalize(vec3(1.0, 1.15, 1.0));
  float nDotL = dot(normal, lightDir);

  // Metal = harder shadows, Fabric/leather = soft wrap
  float wrapAmount = isBronze ? 0.15 : 0.38;
  float diff = max(nDotL * (1.0 - wrapAmount) + wrapAmount, 0.22);

  // Enhance contrast for bronze
  if (isBronze) {
    diff = pow(diff, 0.90);
  }

  color *= diff;
  FragColor = vec4(color, u_alpha);
}
