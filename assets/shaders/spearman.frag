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

float leatherGrain(vec2 p) {
  float grain = noise(p * 10.0) * 0.16;
  float pores = noise(p * 22.0) * 0.08;
  return grain + pores;
}

float fabricWeave(vec2 p) {
  float weaveX = sin(p.x * 60.0);
  float weaveZ = sin(p.z * 60.0);
  return weaveX * weaveZ * 0.05;
}

void main() {
  vec3 color = u_color;
  if (u_useTexture) {
    color *= texture(u_texture, v_texCoord).rgb;
  }

  vec3 normal = normalize(v_normal);
  vec2 uv = v_worldPos.xz * 4.5;
  float avgColor = (color.r + color.g + color.b) / 3.0;

  float colorHue =
      max(max(color.r, color.g), color.b) - min(min(color.r, color.g), color.b);
  bool isMetal =
      (avgColor > 0.45 && avgColor <= 0.65 && colorHue < 0.15);
  bool isLeather = (avgColor > 0.30 && avgColor <= 0.50 && colorHue < 0.20);
  bool isFabric = (avgColor > 0.25 && !isMetal && !isLeather);

  if (isMetal) {
    float metalBrushed = abs(sin(v_worldPos.y * 80.0)) * 0.025;
    float rust = noise(uv * 8.0) * 0.10;
    float dents = noise(uv * 6.0) * 0.035;

    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
    float metalSheen = pow(viewAngle, 9.0) * 0.30;
    float metalFresnel = pow(1.0 - viewAngle, 2.0) * 0.22;

    color += vec3(metalSheen + metalFresnel);
    color += vec3(metalBrushed);
    color -= vec3(rust * 0.35 + dents * 0.25);
  }
  else if (isLeather) {
    float leather = leatherGrain(uv);
    float wear = noise(uv * 4.0) * 0.12 - 0.06;

    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
    float leatherSheen = pow(1.0 - viewAngle, 5.0) * 0.12;

    color *= 1.0 + leather - 0.08 + wear;
    color += vec3(leatherSheen);
  }
  else if (isFabric) {
    float weave = fabricWeave(v_worldPos.xz);
    float fabricFuzz = noise(uv * 18.0) * 0.08;
    float folds = noise(uv * 5.0) * 0.10 - 0.05;

    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
    float fabricSheen = pow(1.0 - viewAngle, 7.0) * 0.10;

    color *= 1.0 + fabricFuzz - 0.04 + folds;
    color += vec3(weave + fabricSheen);
  }
  else {
    float detail = noise(uv * 8.0) * 0.14;
    color *= 1.0 + detail - 0.07;
  }

  color = clamp(color, 0.0, 1.0);

  vec3 lightDir = normalize(vec3(1.0, 1.15, 1.0));
  float nDotL = dot(normal, lightDir);

  float wrapAmount = isMetal ? 0.12 : (isLeather ? 0.25 : 0.35);
  float diff = max(nDotL * (1.0 - wrapAmount) + wrapAmount, 0.20);

  if (isMetal) {
    diff = pow(diff, 0.88);
  }

  color *= diff;
  FragColor = vec4(color, u_alpha);
}
