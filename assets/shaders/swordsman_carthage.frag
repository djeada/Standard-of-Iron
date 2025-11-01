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

// Medieval plate armor articulation lines
float armorPlates(vec2 p, float y) {
  // Horizontal articulation lines (overlapping plates)
  float plateY = fract(y * 6.5);
  float plateLine = smoothstep(0.90, 0.98, plateY) * 0.12;

  // Brass rivet decorations
  float rivetX = fract(p.x * 18.0);
  float rivet = smoothstep(0.48, 0.50, rivetX) * smoothstep(0.52, 0.50, rivetX);
  float rivetPattern = rivet * step(0.92, plateY) * 0.25; // Brass is brighter

  return plateLine + rivetPattern;
}

// Chainmail texture pattern
float chainmailRings(vec2 p) {
  vec2 grid = fract(p * 35.0) - 0.5;
  float ring = length(grid);
  float ringPattern =
      smoothstep(0.35, 0.30, ring) - smoothstep(0.25, 0.20, ring);

  // Offset every other row for interlinked appearance
  vec2 offsetGrid = fract(p * 35.0 + vec2(0.5, 0.0)) - 0.5;
  float offsetRing = length(offsetGrid);
  float offsetPattern =
      smoothstep(0.35, 0.30, offsetRing) - smoothstep(0.25, 0.20, offsetRing);

  return (ringPattern + offsetPattern) * 0.15;
}

void main() {
  vec3 color = u_color;
  if (u_useTexture) {
    color *= texture(u_texture, v_texCoord).rgb;
  }

  vec3 normal = normalize(v_normal);
  vec2 uv = v_worldPos.xz * 5.0;
  float avgColor = (color.r + color.g + color.b) / 3.0;

  // Detect material type by color tone
  float colorHue =
      max(max(color.r, color.g), color.b) - min(min(color.r, color.g), color.b);
  bool isBrass =
      (color.r > color.g * 1.10 && color.r > color.b * 1.10 && avgColor > 0.48);

  // === MEDIEVAL KNIGHT MATERIALS ===

  // POLISHED STEEL PLATE (Great Helm, cuirass, pauldrons, rerebraces) - bright
  // silvery
  if (avgColor > 0.60 && !isBrass) {
    // Mirror-polished steel finish
    float brushedMetal = abs(sin(v_worldPos.y * 88.0)) * 0.024;

    // Battle wear: scratches and dents
    float scratches = noise(uv * 33.0) * 0.019;
    float dents = noise(uv * 7.5) * 0.024;

    // Plate articulation lines and rivets
    float plates = armorPlates(v_worldPos.xz, v_worldPos.y);

    // Strong specular reflections (polished metal)
    float viewAngle = abs(dot(normal, normalize(vec3(0.1, 1.0, 0.3))));
    float fresnel = pow(1.0 - viewAngle, 1.9) * 0.33;
    float specular = pow(viewAngle, 11.5) * 0.50;

    // Environmental reflections (sky dome)
    float skyReflection = (normal.y * 0.5 + 0.5) * 0.10;

    color += vec3(fresnel + skyReflection + specular * 1.8);
    color += vec3(plates);
    color += vec3(brushedMetal);
    color -= vec3(scratches + dents * 0.4);
  }
  // BRASS ACCENTS (rivets, buckles, crosses, decorations) - golden
  else if (isBrass) {
    // Warm metallic brass
    float brassNoise = noise(uv * 22.0) * 0.025;
    float patina = noise(uv * 6.0) * 0.08; // Age darkening

    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
    float brassSheen = pow(viewAngle, 8.5) * 0.32;
    float brassFresnel = pow(1.0 - viewAngle, 2.6) * 0.18;

    color += vec3(brassSheen + brassFresnel);
    color += vec3(brassNoise);
    color -= vec3(patina * 0.5); // Darker in recesses
  }
  // CHAINMAIL AVENTAIL (hanging neck protection) - grey steel rings
  else if (avgColor > 0.40 && avgColor <= 0.60) {
    // Interlocked ring texture
    float rings = chainmailRings(v_worldPos.xz);

    // Chainmail has less shine than plate
    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
    float chainSheen = pow(viewAngle, 6.0) * 0.18;

    // Individual ring highlights
    float ringHighlights = noise(uv * 30.0) * 0.12;

    color += vec3(rings + chainSheen + ringHighlights);
    color *= 1.0 - noise(uv * 12.0) * 0.08; // Slight darkening between rings
  }
  // HERALDIC SURCOAT (team-colored tabard over armor) - bright cloth
  else if (avgColor > 0.25) {
    // Rich fabric weave texture
    float weaveX = sin(v_worldPos.x * 70.0);
    float weaveZ = sin(v_worldPos.z * 70.0);
    float weave = weaveX * weaveZ * 0.04;

    // Embroidered cross emblem texture
    float embroidery = noise(uv * 12.0) * 0.06;

    // Fabric has soft sheen
    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
    float fabricSheen = pow(1.0 - viewAngle, 6.0) * 0.08;

    // Heraldic colors are vibrant
    color *= 1.0 + noise(uv * 5.0) * 0.10 - 0.05;
    color += vec3(weave + embroidery + fabricSheen);
  }
  // LEATHER/DARK ELEMENTS (straps, gloves, scabbard) - dark brown
  else {
    float leatherGrain = noise(uv * 10.0) * 0.15;
    float wearMarks = noise(uv * 3.0) * 0.10;

    color *= 1.0 + leatherGrain - 0.08 + wearMarks - 0.05;
  }

  float seaBlend = saturate((color.g + color.b) * 0.5 - color.r * 0.3) * 0.10;
  color = mix(color, vec3(0.24, 0.58, 0.68), seaBlend);
  color = clamp(color, 0.0, 1.0);

  // Lighting model - hard shadows for metal, soft for fabric
  vec3 lightDir = normalize(vec3(1.0, 1.2, 1.0));
  float nDotL = dot(normal, lightDir);

  // Metal = hard shadows, Fabric = soft wrap
  float wrapAmount = (avgColor > 0.50) ? 0.08 : 0.30;
  float diff = max(nDotL * (1.0 - wrapAmount) + wrapAmount, 0.18);

  // Extra contrast for polished steel
  if (avgColor > 0.60 && !isBrass) {
    diff = pow(diff, 0.85); // Sharper lighting falloff
  }

  color *= diff;
  FragColor = vec4(color, u_alpha);
}
