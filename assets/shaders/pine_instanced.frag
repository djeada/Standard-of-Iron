#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;
in vec2 vTexCoord;
in float vFoliageMask;
in float vNeedleSeed;
in float vBarkSeed;

uniform vec3 uLightDirection;

out vec4 FragColor;

const float PI = 3.14159265359;
const float TWO_PI = 6.28318530718;

float hash(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {

  vec3 n = normalize(vNormal);
  vec3 l = normalize(uLightDirection);
  float diffuse = max(dot(n, l), 0.0);
  float ambient = 0.4;
  float lighting = ambient + diffuse * 0.7;

  float needleNoise = hash(vec2(vTexCoord.x * 28.0 + vNeedleSeed * 7.1,
                                vTexCoord.y * 24.0 + vNeedleSeed * 5.3));

  float needleStreak = hash(vec2(vTexCoord.x * 12.0 + vNeedleSeed * 3.7,
                                 floor(vTexCoord.y * 6.0 + vNeedleSeed * 2.0)));

  vec3 needleColor = vColor * (0.78 + needleNoise * 0.28);
  needleColor += vec3(0.02, 0.05, 0.02) * needleStreak;

  float tipBlend = smoothstep(0.82, 1.02, vTexCoord.y);
  needleColor =
      mix(needleColor, needleColor * vec3(1.08, 1.04, 1.10), tipBlend);

  float barkStripe = sin(vTexCoord.y * 45.0 + vBarkSeed * TWO_PI) * 0.1 + 0.9;
  float barkNoise = hash(vec2(vTexCoord.x * 18.0 + vBarkSeed * 4.3,
                              vTexCoord.y * 10.0 + vBarkSeed * 7.7));

  vec3 trunkBase = vec3(0.32, 0.24, 0.16) * barkStripe;
  vec3 trunkColor = trunkBase * (0.85 + barkNoise * 0.35);

  vec3 baseColor = mix(trunkColor, needleColor, vFoliageMask);
  vec3 color = baseColor * lighting;

  float silhouetteNoise = hash(vec2(vTexCoord.x * 30.0 + vNeedleSeed * 9.0,
                                    vTexCoord.y * 40.0 + vNeedleSeed * 5.5));

  float alphaFoliage = 0.70 + silhouetteNoise * 0.25;
  float alpha = mix(1.0, alphaFoliage, vFoliageMask);

  alpha *= smoothstep(0.00, 0.05, vTexCoord.y);

  alpha = clamp(alpha, 0.0, 1.0);
  if (alpha < 0.06)
    discard;

  FragColor = vec4(color, alpha);
}
