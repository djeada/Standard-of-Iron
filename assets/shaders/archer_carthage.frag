#version 330 core

in vec3 v_normal;
in vec2 v_texCoord;
in vec3 v_worldPos;
in float v_armorLayer; // NEW: Armor layer from vertex shader

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
      (color.r > color.g * 1.02 && color.r > color.b * 1.10 && avgColor > 0.48);
  bool isSeaCloak = (color.g > color.r * 1.2 && color.b > color.r * 1.3);
  bool isLinothorax =
      (v_armorLayer == 1.0 && avgColor > 0.55 && avgColor < 0.78);
  bool isLeatherCap = (v_armorLayer == 0.0 && !isBronze);

  // === CARTHAGINIAN LIGHT ARCHER MATERIALS (North African/Mercenary style) ===

  // LEATHER CAP/HEADBAND (instead of heavy bronze helmet)
  if (isLeatherCap) {
    // Thick tanned leather with Numidian/Libyan styling
    float leatherGrain = noise(uv * 14.0) * 0.20;
    float leatherPores = noise(uv * 28.0) * 0.10;
    float tooledPattern =
        sin(v_worldPos.x * 40.0) * sin(v_worldPos.y * 35.0) * 0.06;

    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.3))));
    float leatherSheen = pow(1.0 - viewAngle, 5.0) * 0.12;

    color *= 1.0 + leatherGrain + leatherPores - 0.08;
    color += vec3(tooledPattern + leatherSheen);
  }
  // LINOTHORAX (LAYERED LINEN ARMOR) - Carthaginian specialty
  else if (isLinothorax) {
    // Multiple layers of glued linen - very distinctive texture
    float linenWeaveX = sin(v_worldPos.x * 65.0);
    float linenWeaveZ = sin(v_worldPos.z * 68.0);
    float weave = linenWeaveX * linenWeaveZ * 0.08;

    // Visible layering from edge-on angles
    float layers = abs(sin(v_worldPos.y * 22.0)) * 0.12;

    // Glue/resin stiffening (darker patches)
    float resinStains = noise(uv * 8.0) * 0.10;

    // Soft matte finish (not shiny like metal)
    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.2))));
    float linenSheen = pow(1.0 - viewAngle, 12.0) * 0.05;

    color *= 1.0 + weave + layers - resinStains * 0.5;
    color += vec3(linenSheen);
  }
  // SUN-DULLED BRONZE (minimal use - only decorative pieces)
  else if (isBronze) {
    float saltPatina = noise(uv * 9.0) * 0.16;
    float verdigris = noise(uv * 12.0) * 0.10;
    float viewAngle = abs(dot(normal, normalize(vec3(0.1, 1.0, 0.2))));
    float bronzeSheen = pow(viewAngle, 6.5) * 0.22;
    float bronzeFresnel = pow(1.0 - viewAngle, 2.0) * 0.20;
    color += vec3(bronzeSheen + bronzeFresnel);
    color -= vec3(saltPatina * 0.4 + verdigris * 0.35);
  }
  // LIGHT LEATHER ARMOR (not chainmail - mercenary style)
  else if (avgColor > 0.35 && avgColor <= 0.58 && !isSeaCloak) {
    // Hardened leather cuirass instead of heavy mail
    float hardenedGrain = noise(uv * 16.0) * 0.18;
    float cracks = noise(uv * 32.0) * 0.08;
    float oilSheen =
        pow(abs(dot(normal, normalize(vec3(0.2, 1.0, 0.3)))), 8.0) * 0.14;

    color += vec3(hardenedGrain + oilSheen);
    color -= vec3(cracks * 0.4);
  }
  // TEAL SEA CLOAK
  else if (isSeaCloak) {
    float weaveX = sin(v_worldPos.x * 48.0);
    float weaveZ = sin(v_worldPos.z * 52.0);
    float weave = weaveX * weaveZ * 0.040;
    float woolFuzz = noise(uv * 19.0) * 0.08;
    float folds = noise(uv * 7.0) * 0.10 - 0.05;
    float capeSheen =
        pow(1.0 - abs(dot(normal, vec3(0.0, 1.0, 0.2))), 7.5) * 0.07;

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
  float wrapAmount = isBronze ? 0.18 : 0.40;
  float diff = max(nDotL * (1.0 - wrapAmount) + wrapAmount, 0.22);

  // Enhance contrast for bronze
  if (isBronze) {
    diff = pow(diff, 0.90);
  }

  color *= diff;
  FragColor = vec4(color, u_alpha);
}
