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

float armorPlates(vec2 p, float y) {

  float plateY = fract(y * 6.5);
  float plateLine = smoothstep(0.90, 0.98, plateY) * 0.12;

  float rivetX = fract(p.x * 18.0);
  float rivet = smoothstep(0.48, 0.50, rivetX) * smoothstep(0.52, 0.50, rivetX);
  float rivetPattern = rivet * step(0.92, plateY) * 0.25;

  return plateLine + rivetPattern;
}

float chainmailRings(vec2 p) {
  vec2 grid = fract(p * 35.0) - 0.5;
  float ring = length(grid);
  float ringPattern =
      smoothstep(0.35, 0.30, ring) - smoothstep(0.25, 0.20, ring);

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

  float colorHue =
      max(max(color.r, color.g), color.b) - min(min(color.r, color.g), color.b);
  bool isBrass =
      (color.r > color.g * 1.15 && color.r > color.b * 1.2 && avgColor > 0.55);

  if (avgColor > 0.60 && !isBrass) {

    float brushedMetal = abs(sin(v_worldPos.y * 95.0)) * 0.02;

    float scratches = noise(uv * 35.0) * 0.018;
    float dents = noise(uv * 8.0) * 0.025;

    float plates = armorPlates(v_worldPos.xz, v_worldPos.y);

    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
    float fresnel = pow(1.0 - viewAngle, 1.8) * 0.35;
    float specular = pow(viewAngle, 12.0) * 0.55;

    float skyReflection = (normal.y * 0.5 + 0.5) * 0.12;

    color += vec3(fresnel + skyReflection + specular * 1.8);
    color += vec3(plates);
    color += vec3(brushedMetal);
    color -= vec3(scratches + dents * 0.4);
  }

  else if (isBrass) {

    float brassNoise = noise(uv * 22.0) * 0.025;
    float patina = noise(uv * 6.0) * 0.08;

    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
    float brassSheen = pow(viewAngle, 8.0) * 0.35;
    float brassFresnel = pow(1.0 - viewAngle, 2.5) * 0.20;

    color += vec3(brassSheen + brassFresnel);
    color += vec3(brassNoise);
    color -= vec3(patina * 0.5);
  }

  else if (avgColor > 0.40 && avgColor <= 0.60) {

    float rings = chainmailRings(v_worldPos.xz);

    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
    float chainSheen = pow(viewAngle, 6.0) * 0.18;

    float ringHighlights = noise(uv * 30.0) * 0.12;

    color += vec3(rings + chainSheen + ringHighlights);
    color *= 1.0 - noise(uv * 12.0) * 0.08;
  }

  else if (avgColor > 0.25) {

    float weaveX = sin(v_worldPos.x * 70.0);
    float weaveZ = sin(v_worldPos.z * 70.0);
    float weave = weaveX * weaveZ * 0.04;

    float embroidery = noise(uv * 12.0) * 0.06;

    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
    float fabricSheen = pow(1.0 - viewAngle, 6.0) * 0.08;

    color *= 1.0 + noise(uv * 5.0) * 0.10 - 0.05;
    color += vec3(weave + embroidery + fabricSheen);
  }

  else {
    float leatherGrain = noise(uv * 10.0) * 0.15;
    float wearMarks = noise(uv * 3.0) * 0.10;

    color *= 1.0 + leatherGrain - 0.08 + wearMarks - 0.05;
  }

  color = clamp(color, 0.0, 1.0);

  vec3 lightDir = normalize(vec3(1.0, 1.2, 1.0));
  float nDotL = dot(normal, lightDir);

  float wrapAmount = (avgColor > 0.50) ? 0.08 : 0.30;
  float diff = max(nDotL * (1.0 - wrapAmount) + wrapAmount, 0.18);

  if (avgColor > 0.60 && !isBrass) {
    diff = pow(diff, 0.85);
  }

  color *= diff;
  FragColor = vec4(color, u_alpha);
}
