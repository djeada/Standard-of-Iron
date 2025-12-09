#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;
in vec2 vTexCoord;
in float vFoliageMask;
in float vLeafSeed;
in float vBarkSeed;
in float vBranchId;
in vec2 vLocalPosXZ;

uniform vec3 uLightDirection;

out vec4 FragColor;

const float PI = 3.14159265359;
const float TWO_PI = 6.28318530718;

float hash(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float hash3(vec3 p) {
  return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

float noise2D(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float a = hash(i);
  float b = hash(i + vec2(1.0, 0.0));
  float c = hash(i + vec2(0.0, 1.0));
  float d = hash(i + vec2(1.0, 1.0));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main() {

  vec3 n = normalize(vNormal);
  vec3 l = normalize(uLightDirection);
  float diffuse = max(dot(n, l), 0.0);
  float ambient = 0.45;
  float lighting = ambient + diffuse * 0.60;

  vec2 leafPos = vLocalPosXZ * 120.0 + vec2(vLeafSeed * 17.3, vBarkSeed * 23.1);

  float leafLayer1 = hash(floor(leafPos));
  float leafLayer2 = hash(floor(leafPos * 1.7 + vec2(5.3, 8.7)));
  float leafLayer3 = hash(floor(leafPos * 0.6 + vec2(13.1, 3.9)));

  float leafDensity = (leafLayer1 + leafLayer2 + leafLayer3) / 3.0;

  float leafEdge = noise2D(leafPos * 2.5);
  float leafFine = hash(leafPos * 0.37 + vec2(vBranchId * 7.0));

  vec3 leafDarkGreen = vec3(0.22, 0.32, 0.20);
  vec3 leafMidGreen = vec3(0.32, 0.42, 0.28);
  vec3 leafLightGreen = vec3(0.42, 0.50, 0.38);
  vec3 leafSilver = vec3(0.52, 0.56, 0.50);

  float colorChoice = leafFine;
  vec3 leafColor = mix(leafDarkGreen, leafMidGreen, colorChoice);

  float backfacing = 1.0 - max(dot(n, l), 0.0);
  float silverShow = smoothstep(0.4, 0.8, backfacing) * leafLayer2;
  leafColor = mix(leafColor, leafSilver, silverShow * 0.5);

  float highlight = smoothstep(0.7, 0.9, leafLayer1 * leafEdge);
  leafColor = mix(leafColor, leafLightGreen, highlight * 0.4);

  leafColor = mix(leafColor, vColor, 0.15);

  float canopyDepth = 1.0 - smoothstep(0.0, 0.35, length(vLocalPosXZ));
  leafColor *= mix(0.75, 1.0, canopyDepth);

  float barkU = vTexCoord.x * TWO_PI;
  float barkV = vTexCoord.y;

  float furrows = pow(abs(sin(barkU * 5.0 + vBarkSeed * TWO_PI)), 0.4);
  float verticalGrain =
      noise2D(vec2(barkU * 3.0, barkV * 25.0 + vBarkSeed * 7.0));
  float barkNoise = noise2D(vec2(barkU * 8.0, barkV * 15.0)) * 0.3;
  float barkTexture = furrows * 0.5 + verticalGrain * 0.35 + barkNoise;

  vec3 barkDark = vec3(0.18, 0.16, 0.13);
  vec3 barkMid = vec3(0.30, 0.27, 0.23);
  vec3 barkLight = vec3(0.42, 0.38, 0.33);

  vec3 barkColor = mix(barkDark, barkMid, barkTexture);
  float barkHighlight =
      smoothstep(0.75, 0.95, hash(vec2(barkV * 15.0, barkU * 3.0)));
  barkColor = mix(barkColor, barkLight, barkHighlight * 0.35);

  vec3 baseColor = mix(barkColor, leafColor, vFoliageMask);
  vec3 color = baseColor * lighting;

  float alpha = 1.0;

  if (vFoliageMask > 0.1) {

    float leafMask = leafDensity;

    float holePattern = noise2D(leafPos * 0.8);
    float holes = smoothstep(0.20, 0.35, holePattern);

    float clusterGaps = noise2D(vLocalPosXZ * 15.0 + vec2(vLeafSeed * 3.0));
    float gaps = smoothstep(0.15, 0.40, clusterGaps);

    float edgeDist = length(vLocalPosXZ);
    float edgeFade = 1.0 - smoothstep(0.25, 0.50, edgeDist) * 0.5;

    float topFade = 1.0 - smoothstep(0.85, 1.0, vTexCoord.y) * 0.4;

    alpha = leafMask * holes * gaps * edgeFade * topFade;
    alpha = mix(1.0, alpha, vFoliageMask);

    if (leafFine > 0.82) {
      alpha *= 0.2;
    }
  }

  alpha *= smoothstep(0.0, 0.06, vTexCoord.y);

  if (alpha < 0.15)
    discard;

  alpha = clamp(alpha * 1.3, 0.0, 1.0);

  FragColor = vec4(color, alpha);
}
