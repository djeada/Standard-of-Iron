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
in float v_steelWear;
in float v_chainmailPhase;
in float v_rivetPattern;
in float v_leatherWear;

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

float chainmailRings(vec2 p) {
  vec2 grid = fract(p * 32.0) - 0.5;
  float ring = length(grid);
  float ringPattern = smoothstep(0.38, 0.32, ring) - smoothstep(0.28, 0.22, ring);
  vec2 offsetGrid = fract(p * 32.0 + vec2(0.5, 0.0)) - 0.5;
  float offsetRing = length(offsetGrid);
  float offsetPattern = smoothstep(0.38, 0.32, offsetRing) - smoothstep(0.28, 0.22, offsetRing);
  return (ringPattern + offsetPattern) * 0.14;
}

float pterugesStrips(vec2 p, float y) {
  float stripX = fract(p.x * 9.0);
  float strip = smoothstep(0.15, 0.20, stripX) - smoothstep(0.80, 0.85, stripX);
  float leatherTex = noise(p * 18.0) * 0.35;
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
  
  // Material ID: 0=body/skin, 1=armor, 2=helmet, 3=weapon, 4=shield
  bool isArmor = (u_materialId == 1);
  bool isHelmet = (u_materialId == 2);
  bool isWeapon = (u_materialId == 3);
  bool isShield = (u_materialId == 4);
  
  // Fallback to old layer system for non-armor meshes
  if (u_materialId == 0) {
    isHelmet = (v_armorLayer < 0.5);
    isArmor = false;  // Body mesh should not get armor effects
  }
  
  bool isLegs = (v_armorLayer >= 1.5);

  // === ROMAN SPEARMAN (HASTATUS) MATERIALS ===

  // HEAVY STEEL HELMET (cool blue-grey steel)
  if (isHelmet) {
    // Steel wear patterns from vertex shader
    float brushed = abs(sin(v_worldPos.y * 95.0)) * 0.020;
    float dents = noise(uv * 6.5) * 0.032 * v_steelWear;
    float rustTex = noise(uv * 9.0) * 0.11 * v_steelWear;
    
    // Use vertex-computed helmet detail (reinforcement bands, brow band, cheek guards)
    float bands = v_helmetDetail * 0.12;
    float rivets = v_rivetPattern * 0.10;
    
    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);
    float steelSheen = pow(viewAngle, 9.0) * 0.30;
    float steelFresnel = pow(1.0 - viewAngle, 2.0) * 0.22;

    color += vec3(steelSheen + steelFresnel + bands + rivets);
    color -= vec3(rustTex * 0.3);
    color += vec3(brushed * 0.6);
  }
  // LIGHT CHAINMAIL ARMOR (lorica hamata - pectorale reinforcement optional)
  else if (isArmor) {
    // FORCE grey metal base - chainmail is CLEARLY not skin
    color = color * vec3(0.42, 0.46, 0.50);  // Darker grey base
    
    vec2 chainUV = v_worldPos.xz * 22.0;  // Larger rings
    
    // PECTORALE CHEST PLATE - highly visible steel plate overlay
    float chestPlate = smoothstep(1.15, 1.25, v_bodyHeight) * 
                       smoothstep(1.55, 1.45, v_bodyHeight) *
                       smoothstep(0.25, 0.15, abs(v_worldPos.x));  // Center chest
    
    // STRONG distinction between plate and chainmail
    if (chestPlate > 0.3) {
      // PECTORALE - polished steel plate
      color = vec3(0.72, 0.76, 0.82);  // Bright steel
      
      // Plate edges and rivets
      float plateEdge = smoothstep(0.88, 0.92, chestPlate) * 0.25;
      float rivets = step(0.92, fract(v_worldPos.x * 18.0)) * 
                     step(0.92, fract(v_worldPos.y * 12.0)) * 0.30;
      
      vec3 V = normalize(vec3(0.0, 1.0, 0.5));
      float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);
      float plateSheen = pow(viewAngle, 8.0) * 0.75;  // Strong reflection
      
      color += vec3(plateSheen + rivets);
      color -= vec3(plateEdge);
    } else {
      // CHAINMAIL - much more visible rings
      float rings = chainmailRings(chainUV) * 2.2;
      float ringGaps = (1.0 - chainmailRings(chainUV)) * 0.50;
      
      // Iron oxidation and rust
      float oxidation = noise(chainUV * 7.5) * 0.28 * v_steelWear;
      vec3 rustColor = vec3(0.38, 0.28, 0.22);
      
      // Battle damage
      float damage = step(0.86, noise(chainUV * 0.8)) * 0.40;
      
      vec3 V = normalize(vec3(0.0, 1.0, 0.5));
      float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);
      float chainSheen = pow(viewAngle, 4.5) * 0.55;
      
      float shimmer = abs(sin(chainUV.x * 32.0) * sin(chainUV.y * 32.0)) * 0.22;
      
      color += vec3(rings + chainSheen + shimmer);
      color -= vec3(ringGaps + damage);
      color = mix(color, rustColor, oxidation * 0.40);
    }
    
    // Ensure armor is ALWAYS clearly visible
    color = clamp(color, vec3(0.32), vec3(0.88));
  }
  // LEATHER PTERUGES & BELT
  else if (isLegs) {
    float leatherGrain = noise(uv * 10.0) * 0.16 * (0.5 + v_leatherWear * 0.5);
    float leatherPores = noise(uv * 22.0) * 0.08;
    float strips = pterugesStrips(v_worldPos.xz, v_bodyHeight);
    float wear = noise(uv * 4.0) * v_leatherWear * 0.10 - 0.05;
    
    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);
    float leatherSheen = pow(1.0 - viewAngle, 4.5) * 0.10;

    color *= 1.0 + leatherGrain + leatherPores - 0.08 + wear;
    color += vec3(strips * 0.15 + leatherSheen);
  }

  color = clamp(color, 0.0, 1.0);

  // Lighting per material
  vec3 lightDir = normalize(vec3(1.0, 1.15, 1.0));
  float nDotL = dot(normalize(v_worldNormal), lightDir);

  float wrapAmount = isHelmet ? 0.12 : (isArmor ? 0.22 : 0.35);
  float diff = max(nDotL * (1.0 - wrapAmount) + wrapAmount, 0.20);

  if (isHelmet) {
    diff = pow(diff, 0.88);
  }

  color *= diff;
  FragColor = vec4(color, u_alpha);
}
