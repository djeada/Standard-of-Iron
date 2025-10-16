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

vec3 proceduralMaterialVariation(vec3 baseColor, vec3 worldPos, vec3 normal) {
  vec2 uv = worldPos.xz * 4.0;

  float avgColor = (baseColor.r + baseColor.g + baseColor.b) / 3.0;

  vec3 variation = baseColor;

  if (avgColor < 0.30) {
    float metalNoise = noise(uv * 8.0) * 0.015;
    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
    float fresnel = pow(1.0 - viewAngle, 2.0) * 0.08;
    variation = baseColor + vec3(metalNoise + fresnel);
  } else if (avgColor > 0.65) {
    float weaveX = sin(worldPos.x * 50.0);
    float weaveZ = sin(worldPos.z * 50.0);
    float weavePattern = weaveX * weaveZ * 0.02;
    float clothNoise = noise(uv * 2.0) * 0.08 - 0.04;
    variation = baseColor * (1.0 + clothNoise + weavePattern);
  } else {
    float leatherNoise = noise(uv * 5.0);
    float blotches = noise(uv * 1.5) * 0.1 - 0.05;
    variation = baseColor * (1.0 + leatherNoise * 0.12 - 0.06 + blotches);
  }

  return clamp(variation, 0.0, 1.0);
}

void main() {
  vec3 color = u_color;
  if (u_useTexture) {
    color *= texture(u_texture, v_texCoord).rgb;
  }

  vec3 normal = normalize(v_normal);
  color = proceduralMaterialVariation(color, v_worldPos, normal);

  vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
  float diff = max(dot(normal, lightDir), 0.2);
  color *= diff;
  FragColor = vec4(color, u_alpha);
}