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
in float v_platePhase;
in float v_segmentStress;
in float v_rivetPattern;
in float v_polishLevel;

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

// Segmented armor plates (lorica segmentata)
float armorPlates(vec2 p, float y) {
  float plateY = fract(y * 6.5);
  float plateLine = smoothstep(0.90, 0.98, plateY) * 0.12;
  float rivetX = fract(p.x * 18.0);
  float rivet = smoothstep(0.48, 0.50, rivetX) * smoothstep(0.52, 0.50, rivetX);
  float rivetPattern = rivet * step(0.92, plateY) * 0.25;
  return plateLine + rivetPattern;
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
  vec2 uv = v_worldPos.xz * 5.0;
  
  bool isHelmet = (v_armorLayer < 0.5);
  bool isArmor = (v_armorLayer >= 0.5 && v_armorLayer < 1.5);
  bool isLegs = (v_armorLayer >= 1.5);

  // === ROMAN SWORDSMAN (LEGIONARY) MATERIALS ===

  // HEAVY STEEL HELMET (galea - cool blue-grey steel)
  if (isHelmet) {
    // Polished steel finish with vertex polish level
    float brushedMetal = abs(sin(v_worldPos.y * 95.0)) * 0.02;
    float scratches = noise(uv * 35.0) * 0.018 * (1.0 - v_polishLevel * 0.5);
    float dents = noise(uv * 8.0) * 0.025;
    
    // Use vertex-computed helmet detail
    float bands = v_helmetDetail * 0.12;
    float rivets = v_rivetPattern * 0.12;
    
    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);
    float fresnel = pow(1.0 - viewAngle, 1.8) * 0.35;
    float specular = pow(viewAngle, 12.0) * 0.55 * v_polishLevel;
    float skyReflection = (v_worldNormal.y * 0.5 + 0.5) * 0.12;

    color += vec3(fresnel + skyReflection + specular * 1.8 + bands + rivets);
    color += vec3(brushedMetal);
    color -= vec3(scratches + dents * 0.4);
  }
  // HEAVY SEGMENTED ARMOR (lorica segmentata - iconic Roman plate armor)
  else if (isArmor) {
    // Historical lorica segmentata construction:
    // - 30-40 curved iron/steel plates in horizontal bands
    // - Girth hoops wrap torso, connected by internal leather straps
    // - Hinged on sides, buckled front/back
    // - ~9kg, superior protection, 1st-2nd century AD peak usage
    
    vec2 armorUV = v_worldPos.xz * 4.8;
    
    // === PLATE BANDS (6-7 horizontal segments) ===
    // Vertex shader computed plate phase for band positioning
    float bandEdge = smoothstep(0.88, 0.96, v_platePhase) + 
                     smoothstep(0.12, 0.04, v_platePhase);
    float plateLine = bandEdge * 0.14;
    
    // Overlap shadows (upper plates overlap lower by ~1cm)
    float overlapShadow = smoothstep(0.92, 0.98, v_platePhase) * 0.16;
    
    // Plate curvature fitted to torso
    float curvature = abs(fract(armorUV.x * 2.5) - 0.5) * 0.08;
    
    // === RIVETS & FASTENERS ===
    // Brass/bronze rivets along edges (~4cm spacing)
    float rivetSpacing = fract(v_worldPos.x * 22.0);
    float rivet = smoothstep(0.47, 0.50, rivetSpacing) * 
                  smoothstep(0.53, 0.50, rivetSpacing);
    float brassRivets = rivet * v_rivetPattern * 0.22;
    
    // Side hinges (vertical connection system)
    float sideHinge = step(0.75, abs(armorUV.x)) * 0.10;
    
    // === SURFACE FINISH ===
    // Brushed/polished steel (well-maintained by legionaries)
    float brushedFinish = abs(sin(v_worldPos.y * 88.0 + v_worldPos.x * 15.0)) * 0.025;
    
    // Scratches (fewer on well-polished armor)
    float scratches = noise(armorUV * 38.0) * 0.022 * (1.0 - v_polishLevel * 0.6);
    float deepScratches = step(0.94, noise(armorUV * 8.5)) * 0.015;
    
    // === BATTLE WEAR ===
    // Articulation wear at segment joints (vertex computed stress)
    float jointWear = v_segmentStress * 0.18;
    
    // Impact dents from frontal combat
    float frontFacing = smoothstep(-0.3, 0.6, v_worldNormal.z);
    float dents = noise(armorUV * 7.5) * 0.028 * frontFacing;
    float majorDent = step(0.92, noise(armorUV * 2.2)) * 0.045 * frontFacing;
    
    // Minimal rust (only in joint crevices if neglected)
    float rustInJoints = (1.0 - v_polishLevel) * 0.12 * 
                         smoothstep(0.95, 1.0, v_platePhase);
    
    // === LIGHTING ===
    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);
    
    // Strong specular on polished plates
    float plateSpecular = pow(viewAngle, 11.0) * 0.52 * v_polishLevel;
    
    // Metallic fresnel
    float plateFresnel = pow(1.0 - viewAngle, 1.9) * 0.32;
    
    // Sky reflection
    float skyReflect = (v_worldNormal.y * 0.5 + 0.5) * 0.14 * v_polishLevel;
    
    // AO in overlaps
    float ao = overlapShadow * 0.20;

    color += vec3(plateLine + brassRivets + sideHinge + plateSpecular + 
                  plateFresnel + skyReflect + brushedFinish + curvature);
    color -= vec3(scratches + deepScratches + jointWear + dents + majorDent + 
                  rustInJoints + ao);
  }
  // LEATHER PTERUGES & BELT
  else if (isLegs) {
    float leatherGrain = noise(uv * 10.0) * 0.15;
    float strips = pterugesStrips(v_worldPos.xz, v_bodyHeight);
    float wearMarks = noise(uv * 3.0) * 0.10;
    
    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);
    float leatherSheen = pow(1.0 - viewAngle, 4.5) * 0.10;

    color *= 1.0 + leatherGrain - 0.08 + wearMarks - 0.05;
    color += vec3(strips * 0.15 + leatherSheen);
  }

  color = clamp(color, 0.0, 1.0);

  // Lighting per material
  vec3 lightDir = normalize(vec3(1.0, 1.2, 1.0));
  float nDotL = dot(normalize(v_worldNormal), lightDir);

  float wrapAmount = isHelmet ? 0.08 : (isArmor ? 0.10 : 0.30);
  float diff = max(nDotL * (1.0 - wrapAmount) + wrapAmount, 0.18);

  // Extra contrast for polished steel
  if (isHelmet || isArmor) {
    diff = pow(diff, 0.85);
  }

  color *= diff;
  FragColor = vec4(color, u_alpha);
}
