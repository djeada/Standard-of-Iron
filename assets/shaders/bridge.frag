#version 330 core

in vec3 v_normal;
in vec2 v_texCoord;
in vec3 v_worldPos;

uniform vec3 u_color;
uniform vec3 u_lightDirection;
uniform sampler2D u_fogTexture;

out vec4 FragColor;

const float PI = 3.14159265359;

float saturate(float x) { return clamp(x, 0.0, 1.0); }

mat2 rot(float a) {
  float c = cos(a), s = sin(a);
  return mat2(c, -s, s, c);
}

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
  float v = 0.0, a = 0.5;
  for (int i = 0; i < 5; ++i) {
    v += a * noise(p);
    p *= 2.0;
    a *= 0.5;
  }
  return v;
}

vec2 hash2(vec2 p) {
  float n = sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123;
  return fract(vec2(n, n * 1.2154));
}

vec2 worleyF(vec2 p) {
  vec2 n = floor(p);
  vec2 f = fract(p);
  float F1 = 1e9;
  float F2 = 1e9;
  for (int j = -1; j <= 1; ++j) {
    for (int i = -1; i <= 1; ++i) {
      vec2 g = vec2(float(i), float(j));
      vec2 o = hash2(n + g);
      vec2 r = (g + o) - f;
      float d = dot(r, r);
      if (d < F1) {
        F2 = F1;
        F1 = d;
      } else if (d < F2) {
        F2 = d;
      }
    }
  }
  return vec2(sqrt(F1), sqrt(F2));
}

float fresnelSchlick(float cosTheta, float F0) {
  return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float ggxSpecular(vec3 N, vec3 V, vec3 L, float rough, float F0) {
  vec3 H = normalize(V + L);
  float NdotV = max(dot(N, V), 0.0);
  float NdotL = max(dot(N, L), 0.0);
  float NdotH = max(dot(N, H), 0.0);
  float VdotH = max(dot(V, H), 0.0);

  float a = max(rough * rough, 0.001);
  float a2 = a * a;
  float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
  float D = a2 / max(PI * denom * denom, 1e-4);

  float k = (a + 1.0);
  k = (k * k) * 0.125;
  float Gv = NdotV / (NdotV * (1.0 - k) + k);
  float Gl = NdotL / (NdotL * (1.0 - k) + k);
  float G = Gv * Gl;

  float F = fresnelSchlick(VdotH, F0);
  return (D * G * F) / max(4.0 * NdotV * NdotL, 1e-4);
}

void main() {

  vec2 uv = v_worldPos.xz * 0.6;

  vec2 F = worleyF(uv * 1.2);
  float edgeMetric = F.y - F.x;
  float stoneMask = smoothstep(0.05, 0.30, edgeMetric);
  float mortarMask = 1.0 - stoneMask;

  vec2 cell = floor(uv * 1.2);
  float cellRnd = hash(cell);
  vec2 local = fract(uv);
  vec2 uvVar = (rot(cellRnd * 6.2831853) * (local - 0.5) + 0.5) + floor(uv);

  float varLow = (fbm(uv * 0.5) - 0.5) * 0.20;
  float varMid = (fbm(uvVar * 3.0) - 0.5) * 0.15;
  float grain = (noise(uvVar * 18.0) - 0.5) * 0.08;

  vec3 stoneColor = u_color * (1.0 + varLow + varMid + grain);
  vec3 mortarColor = u_color * 0.55;

  float crack = smoothstep(0.02, 0.0, abs(noise(uv * 10.0) - 0.5)) * 0.25;
  stoneColor *= (1.0 - crack * stoneMask);

  float cavity = smoothstep(0.0, 0.18, edgeMetric);
  float ao = mix(0.55, 1.0, cavity) * (0.92 + 0.08 * fbm(uv * 2.5));

  float microBump = (fbm(uvVar * 4.0) - 0.5) * 0.06 * stoneMask;
  float macroWarp = (fbm(uv * 1.2) - 0.5) * 0.03 * stoneMask;
  float mortarDip = -0.06 * mortarMask;
  float h = microBump + macroWarp + mortarDip;

  float sx = dFdx(h);
  float sy = dFdy(h);
  float bumpStrength = 14.0;
  vec3 nBump = normalize(vec3(-sx * bumpStrength, 1.0, -sy * bumpStrength));

  vec3 Ng = normalize(v_normal);
  vec3 N = normalize(mix(Ng, nBump, 0.65));

  vec3 L = normalize(u_lightDirection);
  vec3 V = normalize(vec3(0.0, 0.9, 0.4));

  float NdotL = max(dot(N, L), 0.0);
  float diffuse = NdotL;

  float steep = saturate(length(vec2(sx, sy)) * bumpStrength);
  float roughness = clamp(mix(0.65, 0.95, steep), 0.02, 1.0);
  float F0 = 0.035;

  float spec = ggxSpecular(N, V, L, roughness, F0);

  vec3 baseColor = mix(mortarColor, stoneColor, stoneMask);

  vec3 hemiSky = vec3(0.18, 0.24, 0.30);
  vec3 hemiGround = vec3(0.12, 0.10, 0.09);
  float hemi = N.y * 0.5 + 0.5;

  vec3 litColor = baseColor * (0.35 + 0.70 * diffuse) * ao;
  litColor += mix(hemiGround, hemiSky, hemi) * 0.15;
  litColor += vec3(1.0) * spec * 0.25;

  float grime = (1.0 - cavity) * 0.25 * (0.8 + 0.2 * noise(uv * 7.0));
  float gray = dot(litColor, vec3(0.299, 0.587, 0.114));
  litColor = mix(litColor, vec3(gray * 0.9), grime);

  float fogMask = texture(u_fogTexture, v_texCoord).r;
  float fogAmt = 1.0 - fogMask;
  litColor *= mix(1.0, 0.85, fogAmt * 0.5);

  FragColor = vec4(litColor, 1.0);
}
