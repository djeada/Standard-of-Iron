#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;
in vec2 vTexCoord;
in float vAlpha;
in float vHeight;
in float vSeed;
in float vType;
in vec3 vTangent;
in vec3 vBitangent;

uniform vec3 uLightDirection;

out vec4 FragColor;

float h11(float n) { return fract(sin(n) * 43758.5453123); }

float aawidthUV(vec2 uv) {
  vec2 dx = dFdx(uv), dy = dFdy(uv);
  float w = 0.5 * (length(dx) + length(dy));
  float q = exp2(floor(log2(max(w, 1e-6)) + 0.5));
  return clamp(q, 0.0015, 0.0060);
}

float sdf_to_alpha_uv_stable(float sdf, vec2 uv) {
  float w = aawidthUV(uv);

  sdf -= 0.25 * w;

  float a = 1.0 - smoothstep(-w, w, sdf);

  float steps = mix(24.0, 64.0, a);
  a = floor(a * steps + 0.5) / steps;

  return a;
}

float interleavedGradientNoise(vec2 p) {
  float f = dot(p, vec2(0.06711056, 0.00583715));
  return fract(52.9829189 * fract(f));
}

float stableDither(float seed) {
#ifdef DITHER_SCREEN_ANCHORED
  return interleavedGradientNoise(gl_FragCoord.xy);
#else
  return interleavedGradientNoise(floor(vWorldPos.xz * 4.0 + seed * 17.0));
#endif
}

float quantStep(float w) { return exp2(floor(log2(max(w, 1e-6)) + 0.5)); }

float bushSDF(vec2 uv, float seed) {
  vec2 p = (uv - 0.5) * vec2(1.08, 0.96);
  float sdf = 1e9;
  for (int i = 0; i < 5; i++) {
    float fi = float(i);
    float ang = fi * 1.25663706 + seed * 3.7;
    vec2 c = vec2(cos(ang), sin(ang)) * (0.18 + h11(seed * 7.9 + fi) * 0.05);
    float r = 0.30 + h11(seed * 5.7 + fi) * 0.06;
    float d = length(p - c) - r;
    sdf = min(sdf, d);
  }
  return sdf - 0.007;
}

float rosetteSDF(vec2 uv, float seed) {
  vec2 p = uv - 0.5;
  float a = atan(p.y, p.x);
  float r = length(p);
  float petals = mix(10.0, 16.0, h11(seed * 2.7));
  float wave = 0.20 + 0.06 * sin(a * petals + seed * 5.1);
  return (r - wave) - 0.006;
}

float cactusSDF(vec2 uv, float seed) {
  vec2 p = (uv - 0.5) * vec2(0.92, 1.08);
  float sdf = length(p) - 0.48;
  for (int i = 0; i < 3; i++) {
    float fi = float(i);
    float ang = mix(-1.6, 1.6, h11(seed * 3.3 + fi));
    vec2 c = vec2(0.22 * cos(ang), 0.12 + 0.25 * abs(sin(ang)));
    vec2 e = vec2(0.22, 0.30) * mix(0.7, 1.1, h11(seed * 6.1 + fi));
    float d = length((p - c) / e) - 1.0;
    sdf = min(sdf, d);
  }
  return sdf - 0.006;
}

float plantSDF(vec2 uv, float typeVal, float seed) {
  if (typeVal < 0.45)
    return bushSDF(uv, seed);
  if (typeVal < 0.80)
    return rosetteSDF(uv, seed);
  return cactusSDF(uv, seed);
}

vec2 sdfGrad(vec2 uv, float typeVal, float seed, float stepUV) {
  stepUV = quantStep(stepUV);
  vec2 e = vec2(stepUV, 0.0);
  float sx1 = plantSDF(uv + e.xy, typeVal, seed);
  float sx2 = plantSDF(uv - e.xy, typeVal, seed);
  float sy1 = plantSDF(uv + e.yx, typeVal, seed);
  float sy2 = plantSDF(uv - e.yx, typeVal, seed);
  return vec2(sx1 - sx2, sy1 - sy2) * (0.5 / stepUV);
}

void main() {

  float dryness = mix(0.35, 0.92, h11(vSeed * 2.7 + vType * 0.73));
  vec3 lush = vec3(0.17, 0.32, 0.19);
  vec3 dry = vec3(0.46, 0.44, 0.28);
  vec3 base = mix(lush, dry, dryness);
  base = mix(base, vColor, 0.40);
  base *= 0.88;

  vec2 uv2 = (vTexCoord - 0.5) * 2.0;
  float r2 = clamp(dot(uv2, uv2), 0.0, 1.0);
  float z = sqrt(max(1.0 - r2, 0.0));
  vec3 Nbulge =
      normalize(vTangent * uv2.x + vBitangent * uv2.y + vNormal * (z * 1.8));
  vec3 N = normalize(mix(vNormal, Nbulge, 0.85));

  float typeVal = fract(vType);
  float sdf = plantSDF(vTexCoord, typeVal, vSeed);
  float alpha = sdf_to_alpha_uv_stable(sdf, vTexCoord);

  if (alpha <= 0.002)
    discard;

#ifdef USE_HASHED_ALPHA
  {
    float w = aawidthUV(vTexCoord);
    float thin = smoothstep(0.0, 2.0 * w, alpha);
    if (thin < 0.98) {
      if (thin < stableDither(vSeed))
        discard;
      alpha = 1.0;
    }
  }
#endif

  float stepUV = aawidthUV(vTexCoord);
  vec2 g = sdfGrad(vTexCoord, typeVal, vSeed, stepUV);
  vec3 Nshape =
      normalize(vTangent * (-g.x) + vBitangent * (-g.y) + vNormal * 3.0);
  float edgeMix = smoothstep(0.30, 0.0, sdf);
  vec3 Ntemp = normalize(mix(N, Nshape, 0.6 * edgeMix));

  float wAA = aawidthUV(vTexCoord);
  float edge1 = 1.0 - smoothstep(-wAA, wAA, sdf);
  N = normalize(mix(Ntemp, vNormal, edge1 * 0.5));
  float edgeAtten = mix(0.6, 1.0, pow(1.0 - edge1, 1.5));

  vec3 L = normalize(uLightDirection);
  float nl = max(dot(N, L), 0.0);
  float halfLambert = nl * 0.5 + 0.5;
  float wrap = clamp((dot(N, L) + 0.20) / 1.20, 0.0, 1.0);
  float diffuse = mix(halfLambert, wrap, 0.30) * edgeAtten;
  float sss = pow(clamp(dot(-N, L), 0.0, 1.0), 2.2) * 0.22 * edgeAtten;
  float ambient = 0.16;

  float aoStem = mix(0.50, 1.0, smoothstep(0.0, 0.55, vHeight));

  float tip = smoothstep(0.25, 1.0, r2);
  float inner = smoothstep(-2.0 * wAA, -0.2 * wAA, sdf);
  vec3 albedo = base;
  albedo *= mix(1.0, 1.08, tip);
  albedo *= mix(0.95, 1.0, inner);

  vec3 color = albedo * (ambient + diffuse * aoStem) +
               albedo * sss * vec3(1.0, 0.95, 0.85);

  FragColor = vec4(color, alpha);
}
