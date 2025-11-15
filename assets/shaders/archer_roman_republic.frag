#version 330 core

in vec3 v_normal;
in vec3 v_worldNormal;
in vec3 v_tangent;
in vec3 v_bitangent;
in vec2 v_texCoord;
in vec3 v_worldPos;
in float v_armorLayer;
in float v_bodyHeight;
in float v_helmetDetail;
in float v_chainmailPhase;
in float v_leatherWear;
in float v_curvature;

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
  
  bool isHelmet = (v_armorLayer < 0.5);
  bool isArmor = (v_armorLayer >= 0.5 && v_armorLayer < 1.5);
  bool isLegs = (v_armorLayer >= 1.5);

  // === ROMAN ARCHER (SAGITTARIUS) MATERIALS ===

  // LIGHT BRONZE HELMET (warm golden auxiliary helmet)
  if (isHelmet) {
    // Use vertex-computed helmet detail
    float bands = v_helmetDetail * 0.15;
    
    // Warm bronze patina and wear
    float bronzePatina = noise(uv * 8.0) * 0.12;
    float verdigris = noise(uv * 15.0) * 0.08;
    
    // Hammer marks from forging (using vertex curvature)
    float hammerMarks = noise(uv * 25.0) * 0.035 * (1.0 - v_curvature * 0.3);
    
    // Conical shape highlight
    float apex = smoothstep(0.85, 1.0, v_bodyHeight) * 0.12;
    
    // Bronze sheen using tangent space
    vec3 N = normalize(v_worldNormal);
    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float viewAngle = max(dot(N, V), 0.0);
    float bronzeSheen = pow(viewAngle, 7.0) * 0.25;
    float bronzeFresnel = pow(1.0 - viewAngle, 2.2) * 0.18;

    color += vec3(bronzeSheen + bronzeFresnel + bands + apex);
    color -= vec3(bronzePatina * 0.4 + verdigris * 0.3);
    color += vec3(hammerMarks * 0.5);
  }
  // LIGHT CHAINMAIL ARMOR (lorica hamata - historically accurate 4-in-1 pattern)
  else if (isArmor) {
    // Roman chainmail: butted iron rings, 8-10mm diameter, 1.2mm wire
    // Each ring passes through 4 neighbors (European 4-in-1 pattern)
    vec2 chainUV = v_worldPos.xz * 28.0;
    
    // Primary ring pattern with vertex-computed phase alignment
    float rings = chainmailRings(chainUV) * (0.75 + v_chainmailPhase * 0.50);
    
    // Ring depth - butted rings have visible gaps
    float ringDepth = noise(chainUV * 1.8) * 0.10;
    float gaps = step(0.88, fract(chainUV.x * 32.0)) * 
                 step(0.88, fract(chainUV.y * 32.0)) * 0.12;
    
    // Oxidation in ring joints (iron rusts easily)
    float oxidation = noise(chainUV * 9.0) * 0.14;
    float jointDark = (rings + gaps) * 0.18;
    
    // Battle damage - stretched/broken rings
    float damageSeed = noise(chainUV * 0.7);
    float damage = step(0.94, damageSeed) * 0.20;
    float stretching = noise(chainUV * 2.5) * 0.06;
    
    // Maintenance oil sheen (Romans used oil/animal fat against rust)
    float oilPattern = noise(chainUV * 3.5) * 0.10;
    
    // Specular highlights on ring curves
    vec3 N = normalize(v_worldNormal);
    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float viewAngle = max(dot(N, V), 0.0);
    float chainSheen = pow(viewAngle, 6.5) * 0.22;
    
    // Ambient occlusion between overlapping rings
    float ao = (1.0 - rings * 0.65) * 0.16;
    
    // Micro-scratches from ring friction
    float scratches = noise(chainUV * 48.0) * 0.07;

    color += vec3(rings + chainSheen + oilPattern * 0.6);
    color -= vec3(oxidation * 0.45 + jointDark + ao + damage + stretching);
    color *= 1.0 - scratches;
  }
  // LEATHER PTERUGES & BELT (tan/brown leather strips)
  else if (isLegs) {
    // Thick leather with visible grain (using vertex wear data)
    float leatherGrain = noise(uv * 10.0) * 0.16 * (0.5 + v_leatherWear * 0.5);
    float leatherPores = noise(uv * 22.0) * 0.08;

    // Pteruges strip pattern
    float strips = pterugesStrips(v_worldPos.xz, v_bodyHeight);

    // Worn leather edges
    float wear = noise(uv * 4.0) * v_leatherWear * 0.10 - 0.05;

    // Leather has subtle sheen
    vec3 N = normalize(v_worldNormal);
    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float viewAngle = max(dot(N, V), 0.0);
    float leatherSheen = pow(1.0 - viewAngle, 4.5) * 0.10;

    color *= 1.0 + leatherGrain + leatherPores - 0.08 + wear;
    color += vec3(strips * 0.15 + leatherSheen);
  }

  color = clamp(color, 0.0, 1.0);

  // Lighting model per material
  vec3 lightDir = normalize(vec3(1.0, 1.15, 1.0));
  float nDotL = dot(normal, lightDir);

  float wrapAmount = isHelmet ? 0.15 : (isArmor ? 0.22 : 0.38);
  float diff = max(nDotL * (1.0 - wrapAmount) + wrapAmount, 0.22);

  if (isHelmet) {
    diff = pow(diff, 0.90);
  }

  color *= diff;
  FragColor = vec4(color, u_alpha);
}
