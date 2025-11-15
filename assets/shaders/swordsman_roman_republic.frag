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
  // HEAVY SEGMENTED ARMOR (lorica segmentata - banded plate)
  else if (isArmor) {
    // Use vertex-computed plate phase for articulation
    float plateLine = smoothstep(0.90, 0.98, v_platePhase) * 0.12;
    
    // Rivets with vertex rivet pattern
    float rivetX = fract(v_worldPos.x * 18.0);
    float rivet = smoothstep(0.48, 0.50, rivetX) * smoothstep(0.52, 0.50, rivetX);
    float rivetPattern = rivet * v_rivetPattern * 0.25;
    
    // Battle wear concentrated at segment joints
    float battleWear = noise(uv * 35.0) * 0.020 * v_segmentStress;
    float brushed = abs(sin(v_worldPos.y * 85.0)) * 0.022;
    
    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float viewAngle = max(dot(normalize(v_worldNormal), V), 0.0);
    float plateFresnel = pow(1.0 - viewAngle, 2.0) * 0.28;
    float plateSpecular = pow(viewAngle, 10.0) * 0.45 * v_polishLevel;

    color += vec3(plateLine + rivetPattern + plateFresnel + plateSpecular);
    color += vec3(brushed);
    color -= vec3(battleWear);
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
