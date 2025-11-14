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

float clothWeave(vec2 p) {
  float warpThread = sin(p.x * 70.0);
  float weftThread = sin(p.y * 68.0);
  return warpThread * weftThread * 0.06;
}

void main() {
  vec3 color = u_color;
  if (u_useTexture) {
    color *= texture(u_texture, v_texCoord).rgb;
  }

  vec3 normal = normalize(v_normal);
  vec2 uv = v_worldPos.xz * 4.5;
  float avgColor = (color.r + color.g + color.b) / 3.0;

  // Light cloth material (healer robes)
  if (avgColor > 0.65) {
    float weave = clothWeave(v_worldPos.xz);
    float folds = noise(uv * 8.0) * 0.13;
    
    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.2))));
    float clothSheen = pow(1.0 - viewAngle, 9.0) * 0.12;

    color *= 1.0 + weave + folds - 0.03;
    color += vec3(clothSheen);
  }
  // Leather/darker elements
  else if (avgColor > 0.30) {
    float leatherGrain = noise(uv * 14.0) * 0.14;
    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.4))));
    float leatherSheen = pow(1.0 - viewAngle, 6.0) * 0.10;

    color *= 1.0 + leatherGrain - 0.04;
    color += vec3(leatherSheen);
  }
  else {
    float detail = noise(uv * 10.0) * 0.10;
    color *= 1.0 + detail - 0.06;
  }

  color = clamp(color, 0.0, 1.0);

  vec3 lightDir = normalize(vec3(1.0, 1.2, 1.0));
  float nDotL = dot(normal, lightDir);
  float wrapAmount = 0.45;
  float diff = max(nDotL * (1.0 - wrapAmount) + wrapAmount, 0.28);

  color *= diff;
  FragColor = vec4(color, u_alpha);
}
