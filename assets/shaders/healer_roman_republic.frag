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

float clothWeave(vec2 p) {
  float warpThread = sin(p.x * 70.0);
  float weftThread = sin(p.y * 68.0);
  return warpThread * weftThread * 0.06;
}

float romanLinen(vec2 p) {
  float weave = clothWeave(p);
  float fineThread = noise(p * 90.0) * 0.07;
  return weave + fineThread;
}

void main() {
  vec3 color = u_color;
  if (u_useTexture) {
    color *= texture(u_texture, v_texCoord).rgb;
  }

  vec3 normal = normalize(v_normal);
  vec2 uv = v_worldPos.xz * 4.5;
  float avgColor = (color.r + color.g + color.b) / 3.0;

  bool isWhite = (avgColor > 0.75);
  bool isRed = (color.r > color.g * 1.18 && color.r > color.b * 1.15);
  bool isHelmet = (v_armorLayer == 0.0);

  // WHITE TUNICA (Roman medicus robe)
  if (isWhite || avgColor > 0.72) {
    float linen = romanLinen(v_worldPos.xz);
    float romanFolds = noise(uv * 9.0) * 0.13;

    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.2))));
    float linenSheen = pow(1.0 - viewAngle, 10.0) * 0.12;

    color *= 1.0 + linen + romanFolds - 0.03;
    color += vec3(linenSheen);
  }
  // RED CAPE/TRIM (Roman military medicus)
  else if (isRed) {
    float weave = clothWeave(v_worldPos.xz);
    float dyeRichness = noise(uv * 5.0) * 0.14;

    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.3))));
    float woolSheen = pow(1.0 - viewAngle, 7.0) * 0.11;

    color *= 1.0 + weave + dyeRichness - 0.04;
    color += vec3(woolSheen);
  }
  // LEATHER ELEMENTS (sandals, belt, bag)
  else if (avgColor > 0.32 && avgColor <= 0.58) {
    float leatherGrain = noise(uv * 15.0) * 0.15;
    float romanTooling = noise(uv * 22.0) * 0.05;

    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.4))));
    float leatherSheen = pow(1.0 - viewAngle, 5.5) * 0.11;

    color *= 1.0 + leatherGrain + romanTooling - 0.05;
    color += vec3(leatherSheen);
  } else {
    float detail = noise(uv * 10.0) * 0.11;
    color *= 1.0 + detail - 0.06;
  }

  color = clamp(color, 0.0, 1.0);

  vec3 lightDir = normalize(vec3(1.0, 1.2, 1.0));
  float nDotL = dot(normal, lightDir);
  float wrapAmount = 0.44;
  float diff = max(nDotL * (1.0 - wrapAmount) + wrapAmount, 0.27);

  color *= diff;
  FragColor = vec4(color, u_alpha);
}
