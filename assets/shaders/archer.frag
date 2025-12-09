#version 330 core

in vec3 v_normal;
in vec2 v_texCoord;
in vec3 v_worldPos;
in float v_armorLayer;

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

float chainmailRings(vec2 p) {
  vec2 grid = fract(p * 32.0) - 0.5;
  float ring = length(grid);
  float ringPattern =
      smoothstep(0.38, 0.32, ring) - smoothstep(0.28, 0.22, ring);

  vec2 offsetGrid = fract(p * 32.0 + vec2(0.5, 0.0)) - 0.5;
  float offsetRing = length(offsetGrid);
  float offsetPattern =
      smoothstep(0.38, 0.32, offsetRing) - smoothstep(0.28, 0.22, offsetRing);

  return (ringPattern + offsetPattern) * 0.14;
}

float pterugesStrips(vec2 p, float y) {

  float stripX = fract(p.x * 9.0);
  float strip = smoothstep(0.15, 0.20, stripX) - smoothstep(0.80, 0.85, stripX);

  float leatherTex = noise(p * 18.0) * 0.35;

  float hang = smoothstep(0.65, 0.45, y);

  return strip * leatherTex * hang;
}

float scaleArmor(vec2 p, float y) {

  vec2 scaleGrid = p * 18.0;
  scaleGrid.y += sin(scaleGrid.x * 2.0) * 0.15;

  vec2 scaleFract = fract(scaleGrid) - 0.5;

  float scaleDist = length(scaleFract * vec2(1.0, 1.4));
  float scaleShape = smoothstep(0.48, 0.38, scaleDist);

  float overlap = sin(scaleGrid.y * 3.14159) * 0.08;

  float edge =
      smoothstep(0.42, 0.38, scaleDist) - smoothstep(0.38, 0.34, scaleDist);

  return scaleShape * 0.15 + overlap + edge * 0.12;
}

void main() {
  vec3 color = u_color;
  if (u_useTexture) {
    color *= texture(u_texture, v_texCoord).rgb;
  }

  vec3 normal = normalize(v_normal);
  vec2 uv = v_worldPos.xz * 4.5;
  float avgColor = (color.r + color.g + color.b) / 3.0;

  bool isBronze =
      (color.r > color.g * 1.08 && color.r > color.b * 1.15 && avgColor > 0.50);
  bool isRedCape = (color.r > color.g * 1.3 && color.r > color.b * 1.4);
  bool isHelmet = (v_armorLayer == 0.0);
  bool isChainmail =
      (v_armorLayer == 1.0 && avgColor > 0.40 && avgColor <= 0.60);
  bool isScaleArmor =
      (v_armorLayer == 1.0 && avgColor > 0.50 && avgColor <= 0.70);

  if (isBronze && isHelmet) {

    float bronzePatina = noise(uv * 8.0) * 0.12;
    float verdigris = noise(uv * 15.0) * 0.08;

    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
    float bronzeSheen = pow(viewAngle, 7.0) * 0.25;
    float bronzeFresnel = pow(1.0 - viewAngle, 2.2) * 0.18;

    float hammerMarks = noise(uv * 25.0) * 0.035;

    color += vec3(bronzeSheen + bronzeFresnel);
    color -= vec3(bronzePatina * 0.4 + verdigris * 0.3);
    color += vec3(hammerMarks * 0.5);
  }

  else if (isScaleArmor && !isRedCape) {

    float scales = scaleArmor(v_worldPos.xz, v_worldPos.y);

    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
    float scaleSheen = pow(viewAngle, 6.0) * 0.20;

    float wearBetweenScales = noise(uv * 12.0) * 0.09;

    color += vec3(scales + scaleSheen);
    color -= vec3(wearBetweenScales * 0.35);
    color *= 1.0 - noise(uv * 20.0) * 0.05;
  }

  else if (isChainmail && !isRedCape) {

    float rings = chainmailRings(v_worldPos.xz);

    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
    float chainSheen = pow(viewAngle, 5.0) * 0.16;

    float rust = noise(uv * 10.0) * 0.08;

    color += vec3(rings + chainSheen);
    color -= vec3(rust * 0.4);
    color *= 1.0 - noise(uv * 18.0) * 0.06;
  }

  else if (isRedCape) {

    float weaveX = sin(v_worldPos.x * 55.0);
    float weaveZ = sin(v_worldPos.z * 55.0);
    float weave = weaveX * weaveZ * 0.045;

    float woolFuzz = noise(uv * 20.0) * 0.10;

    float folds = noise(uv * 6.0) * 0.12 - 0.06;

    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
    float capeSheen = pow(1.0 - viewAngle, 8.0) * 0.08;

    color *= 1.0 + woolFuzz - 0.05 + folds;
    color += vec3(weave + capeSheen);
  }

  else if (avgColor > 0.35) {

    float leatherGrain = noise(uv * 10.0) * 0.16;
    float leatherPores = noise(uv * 22.0) * 0.08;

    float strips = pterugesStrips(v_worldPos.xz, v_worldPos.y);

    float wear = noise(uv * 4.0) * 0.10 - 0.05;

    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
    float leatherSheen = pow(1.0 - viewAngle, 4.5) * 0.10;

    color *= 1.0 + leatherGrain + leatherPores - 0.08 + wear;
    color += vec3(strips * 0.15 + leatherSheen);
  }

  else {
    float leatherDetail = noise(uv * 8.0) * 0.14;
    float tooling = noise(uv * 16.0) * 0.06;
    float darkening = noise(uv * 2.5) * 0.08;

    color *= 1.0 + leatherDetail - 0.07 + tooling - darkening;
  }

  color = clamp(color, 0.0, 1.0);

  vec3 lightDir = normalize(vec3(1.0, 1.15, 1.0));
  float nDotL = dot(normal, lightDir);

  float wrapAmount = isBronze ? 0.15 : 0.38;
  float diff = max(nDotL * (1.0 - wrapAmount) + wrapAmount, 0.22);

  if (isBronze) {
    diff = pow(diff, 0.90);
  }

  color *= diff;
  FragColor = vec4(color, u_alpha);
}
