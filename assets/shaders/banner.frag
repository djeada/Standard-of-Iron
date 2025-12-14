#version 330 core

in vec3 v_normal;
in vec2 v_texCoord;
in vec3 v_worldPos;
in float v_waveOffset;
in float v_clothDepth;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform vec3 u_trimColor;
uniform bool u_useTexture;
uniform float u_alpha;
uniform float u_time;

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

float fbm(vec2 p) {
  float value = 0.0;
  float amplitude = 0.5;
  for (int i = 0; i < 3; i++) {
    value += amplitude * noise(p);
    p *= 2.0;
    amplitude *= 0.5;
  }
  return value;
}

float clothWeave(vec2 p) {
  float warp = sin(p.x * 80.0) * 0.5 + 0.5;
  float weft = sin(p.y * 80.0) * 0.5 + 0.5;
  return warp * weft * 0.08;
}

void main() {
  vec3 color = u_color;
  if (u_useTexture) {
    color *= texture(u_texture, v_texCoord).rgb;
  }

  vec3 normal = normalize(v_normal);
  vec2 uv = v_worldPos.xz;

  float weave = clothWeave(uv);

  float foldShadow = 1.0 - abs(v_waveOffset) * 3.0;
  foldShadow = clamp(foldShadow, 0.7, 1.0);

  float fabricNoise = fbm(uv * 15.0) * 0.08 - 0.04;

  vec3 viewDir = normalize(vec3(0.0, 1.0, 0.5));
  float viewAngle = abs(dot(normal, viewDir));
  float sheen = pow(1.0 - viewAngle, 4.0) * 0.2;

  color = color * (1.0 + fabricNoise + weave) * foldShadow;
  color += vec3(sheen);

  float borderWidth = 0.08;
  float edgeDist = min(min(v_texCoord.x, 1.0 - v_texCoord.x),
                       min(v_texCoord.y, 1.0 - v_texCoord.y));
  if (edgeDist < borderWidth) {
    float trimBlend = smoothstep(0.0, borderWidth, edgeDist);
    color = mix(u_trimColor, color, trimBlend);
  }

  vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
  float nDotL = dot(normal, lightDir);

  float wrapAmount = 0.5;
  float diff = max(nDotL * (1.0 - wrapAmount) + wrapAmount, 0.25);

  float ao = 1.0 - v_clothDepth * 0.15;

  color *= diff * ao;

  color = mix(color, color * 0.92, v_clothDepth * 0.3);

  FragColor = vec4(clamp(color, 0.0, 1.0), u_alpha);
}
