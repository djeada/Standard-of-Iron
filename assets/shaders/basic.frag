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

vec3 proceduralMaterialVariation(vec3 baseColor, vec3 worldPos) {
  vec2 uv = worldPos.xz * 4.0;
  
  float n = noise(uv);
  
  float avgColor = (baseColor.r + baseColor.g + baseColor.b) / 3.0;
  
  vec3 variation = baseColor;
  if (avgColor < 0.25) {
    float metalNoise = noise(uv * 2.5);
    variation += vec3(metalNoise * 0.08);
  } else if (avgColor > 0.7) {
    float clothNoise = noise(uv * 1.5);
    variation *= (1.0 + clothNoise * 0.12 - 0.06);
  } else {
    float leatherNoise = noise(uv * 3.0);
    variation *= (1.0 + leatherNoise * 0.15 - 0.075);
  }
  
  return variation;
}

void main() {
  vec3 color = u_color;
  if (u_useTexture) {
    color *= texture(u_texture, v_texCoord).rgb;
  }
  
  color = proceduralMaterialVariation(color, v_worldPos);
  
  vec3 normal = normalize(v_normal);
  vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
  float diff = max(dot(normal, lightDir), 0.2);
  color *= diff;
  FragColor = vec4(color, u_alpha);
}