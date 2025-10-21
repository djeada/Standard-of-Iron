#version 330 core

in vec3 v_normal;
in vec2 v_texCoord;
in vec3 v_worldPos;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_useTexture;
uniform float u_alpha;

out vec4 FragColor;

// ---------------------
// utilities & noise
// ---------------------
const float PI = 3.14159265359;

float saturate(float x){ return clamp(x, 0.0, 1.0); }
vec3  saturate(vec3  x){ return clamp(x, 0.0, 1.0); }

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
  float a = 0.5;
  float f = 0.0;
  for(int i=0;i<5;++i){
    f += a * noise(p);
    p *= 2.03;
    a *= 0.5;
  }
  return f;
}

// anti-aliased step
float aaStep(float edge, float x) {
  float w = fwidth(x);
  return smoothstep(edge - w, edge + w, x);
}

// ---------------------
// patterns
// ---------------------

// plate seams + rivets (AA)
float armorPlates(vec2 p, float y) {
  float plateY = fract(y * 6.5);
  float line = smoothstep(0.92, 0.98, plateY) - smoothstep(0.98, 1.0, plateY);
  // anti-aliased line thickness
  line = smoothstep(0.0, fwidth(plateY) * 2.0, line) * 0.12;

  // rivets on top seams
  float rivetX = fract(p.x * 18.0);
  float rivet = smoothstep(0.48, 0.50, rivetX) * smoothstep(0.52, 0.50, rivetX);
  rivet *= step(0.92, plateY);
  return line + rivet * 0.25;
}

// linked ring suggestion (AA)
float chainmailRings(vec2 p) {
  vec2 uv = p * 35.0;

  vec2 g0 = fract(uv) - 0.5;
  float r0 = length(g0);
  float fw0 = fwidth(r0) * 1.2;
  float ring0 = smoothstep(0.30 + fw0, 0.30 - fw0, r0) - smoothstep(0.20 + fw0, 0.20 - fw0, r0);

  vec2 g1 = fract(uv + vec2(0.5, 0.0)) - 0.5;
  float r1 = length(g1);
  float fw1 = fwidth(r1) * 1.2;
  float ring1 = smoothstep(0.30 + fw1, 0.30 - fw1, r1) - smoothstep(0.20 + fw1, 0.20 - fw1, r1);

  return (ring0 + ring1) * 0.15;
}

float horseHidePattern(vec2 p) {
  float grain = fbm(p * 80.0) * 0.10;
  float ripple = sin(p.x * 22.0) * cos(p.y * 28.0) * 0.035;
  float mottling = smoothstep(0.55, 0.65, fbm(p * 6.0)) * 0.07;
  return grain + ripple + mottling;
}

// ---------------------
// microfacet shading
// ---------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
  return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float D_GGX(float NdotH, float rough) {
  float a  = max(0.001, rough);
  float a2 = a * a;
  float d  = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
  return a2 / max(1e-6, (PI * d * d));
}

float G_Smith(float NdotV, float NdotL, float rough) {
  float r = rough + 1.0;
  float k = (r * r) / 8.0;
  float gV = NdotV / (NdotV * (1.0 - k) + k);
  float gL = NdotL / (NdotL * (1.0 - k) + k);
  return gV * gL;
}

// screen-space bump from a height field h(uv) in world XZ
vec3 perturbNormalWS(vec3 N, vec3 worldPos, float h, float scale) {
  vec3 dpdx = dFdx(worldPos);
  vec3 dpdy = dFdy(worldPos);
  vec3 T = normalize(dpdx);
  vec3 B = normalize(cross(N, T));
  float hx = dFdx(h);
  float hy = dFdy(h);
  vec3 Np = normalize(N - scale * (hx * B + hy * T));
  return Np;
}

// hemisphere ambient (sky/ground)
vec3 hemilight(vec3 N) {
  vec3 sky    = vec3(0.55, 0.64, 0.80);
  vec3 ground = vec3(0.23, 0.20, 0.17);
  float t = saturate(N.y * 0.5 + 0.5);
  return mix(ground, sky, t) * 0.28;
}

// ---------------------
// main
// ---------------------
void main() {
  vec3 baseColor = u_color;
  if (u_useTexture) baseColor *= texture(u_texture, v_texCoord).rgb;

  vec3 N = normalize(v_normal);
  vec2 uv = v_worldPos.xz * 5.0;

  float avg = (baseColor.r + baseColor.g + baseColor.b) * (1.0/3.0);
  float hueSpan = max(max(baseColor.r, baseColor.g), baseColor.b) - min(min(baseColor.r, baseColor.g), baseColor.b);

  bool isBrass     = (baseColor.r > baseColor.g * 1.15 && baseColor.r > baseColor.b * 1.20 && avg > 0.50);
  bool isSteel     = (avg > 0.60 && !isBrass);
  bool isChain     = (!isSteel && !isBrass && avg > 0.40 && avg <= 0.60);
  bool isFabric    = (!isSteel && !isBrass && !isChain && avg > 0.25);
  bool isLeather   = (!isSteel && !isBrass && !isChain && !isFabric);
  bool isHorseHide = (avg < 0.40 && hueSpan < 0.12 && v_worldPos.y < 0.8);

  // lighting frame
  vec3 L = normalize(vec3(1.0, 1.2, 1.0));
  vec3 V = normalize(vec3(0.0, 1.0, 0.5));        // stable view proxy (keeps interface unchanged)
  vec3 H = normalize(L + V);

  float NdotL = saturate(dot(N, L));
  float NdotV = saturate(dot(N, V));
  float NdotH = saturate(dot(N, H));
  float VdotH = saturate(dot(V, H));

  // wrap diffuse like original (softens lambert)
  float wrapAmount = (avg > 0.50) ? 0.08 : 0.30;
  float NdotL_wrap = max(NdotL * (1.0 - wrapAmount) + wrapAmount, 0.12);

  // base material params
  float roughness = 0.5;
  vec3  F0 = vec3(0.04);     // dielectric default
  float metalness = 0.0;
  vec3  albedo = baseColor;

  // micro details / masks (re-used)
  float nSmall = fbm(uv * 6.0);
  float nLarge = fbm(uv * 2.0);
  float cavity = 1.0 - (nLarge * 0.25 + nSmall * 0.15);

  // ---------------------
  // MATERIAL BRANCHES
  // ---------------------
  vec3 col = vec3(0.0);
  vec3 ambient = hemilight(N) * (0.85 + 0.15 * cavity);

  if (isHorseHide) {
    // subtle anisotropic sheen along body flow
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 T = normalize(cross(up, N) + 1e-4); // hair tangent guess
    float flowNoise = fbm(uv * 10.0);
    float aniso = pow(saturate(dot(normalize(reflect(-L, N)), T)), 14.0) * 0.08 * (0.6 + 0.4 * flowNoise);

    float hideTex = horseHidePattern(v_worldPos.xz);
    float sheen   = pow(1.0 - NdotV, 4.0) * 0.07;

    roughness = 0.58 - hideTex * 0.08;
    F0 = vec3(0.035);
    metalness = 0.0;

    // slight bump from hair grain
    float h = fbm(v_worldPos.xz * 35.0);
    N = perturbNormalWS(N, v_worldPos, h, 0.35);

    // composition
    albedo = albedo * (1.0 + hideTex * 0.20) * (0.98 + 0.02 * nSmall);
    col += ambient * albedo;
    // microfacet spec
    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3  F = fresnelSchlick(VdotH, F0);
    vec3  spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);
    col += NdotL_wrap * (albedo * (1.0 - F) * 0.95) + spec * 0.8;
    col += aniso + sheen;

  } else if (isSteel) {
    float brushed = abs(sin(v_worldPos.y * 95.0)) * 0.02 + noise(uv * 35.0) * 0.015;
    float dents   = noise(uv * 8.0) * 0.03;
    float plates  = armorPlates(v_worldPos.xz, v_worldPos.y);

    // bump from brushing
    float h = fbm(vec2(v_worldPos.y * 25.0, v_worldPos.z * 6.0));
    N = perturbNormalWS(N, v_worldPos, h, 0.35);

    // steel-like params
    metalness = 1.0;
    F0 = vec3(0.92);
    roughness = 0.28 + brushed * 2.0 + dents * 0.6;
    roughness = clamp(roughness, 0.15, 0.55);

    // base tint & sky reflection lift
    albedo = mix(vec3(0.60), baseColor, 0.25);
    float skyRefl = (N.y * 0.5 + 0.5) * 0.10;

    // microfacet spec only for metals
    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3  F = fresnelSchlick(VdotH, F0 * albedo); // slight tint
    vec3  spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    col += ambient * 0.3; // metals rely more on spec
    col += NdotL_wrap * spec * 1.5;
    col += vec3(plates) + vec3(skyRefl) - vec3(dents * 0.25) + vec3(brushed);

  } else if (isBrass) {
    float brassNoise = noise(uv * 22.0) * 0.02;
    float patina = fbm(uv * 4.0) * 0.12; // larger-scale patina

    // bump from subtle hammering
    float h = fbm(uv * 18.0);
    N = perturbNormalWS(N, v_worldPos, h, 0.30);

    metalness = 1.0;
    vec3 brassTint = vec3(0.94, 0.78, 0.45);
    F0 = mix(brassTint, baseColor, 0.5);
    roughness = clamp(0.32 + patina * 0.45, 0.18, 0.75);

    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3  F = fresnelSchlick(VdotH, F0);
    vec3  spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    col += ambient * 0.25;
    col += NdotL_wrap * spec * 1.35;
    col += vec3(brassNoise) - vec3(patina * 0.35);

  } else if (isChain) {
    float rings = chainmailRings(v_worldPos.xz);
    float ringHi = noise(uv * 30.0) * 0.10;

    // small pitted bump
    float h = fbm(uv * 35.0);
    N = perturbNormalWS(N, v_worldPos, h, 0.25);

    metalness = 1.0;
    F0 = vec3(0.86);
    roughness = 0.35;

    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3  F = fresnelSchlick(VdotH, F0);
    vec3  spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    col += ambient * 0.25;
    col += NdotL_wrap * (spec * (1.2 + rings)) + vec3(ringHi);
    // slight diffuse damping to keep chainmail darker in cavities
    col *= (0.95 - 0.10 * (1.0 - cavity));

  } else if (isFabric) {
    float weaveX = sin(v_worldPos.x * 70.0);
    float weaveZ = sin(v_worldPos.z * 70.0);
    float weave  = weaveX * weaveZ * 0.04;
    float embroidery = fbm(uv * 6.0) * 0.08;

    float h = fbm(uv * 22.0) * 0.7 + weave * 0.6;
    N = perturbNormalWS(N, v_worldPos, h, 0.35);

    roughness = 0.78;
    F0 = vec3(0.035);
    metalness = 0.0;

    vec3 F = fresnelSchlick(VdotH, F0);
    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3  spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    float sheen = pow(1.0 - NdotV, 6.0) * 0.10;
    albedo *= 1.0 + fbm(uv * 5.0) * 0.10 - 0.05;

    col += ambient * albedo;
    col += NdotL_wrap * (albedo * (1.0 - F) + spec * 0.3) + vec3(weave + embroidery + sheen);

  } else { // leather
    float grain = fbm(uv * 10.0) * 0.15;
    float wear  = fbm(uv * 3.0) * 0.12;

    float h = fbm(uv * 18.0);
    N = perturbNormalWS(N, v_worldPos, h, 0.28);

    roughness = 0.58 - wear * 0.15;
    F0 = vec3(0.038);
    metalness = 0.0;

    vec3 F = fresnelSchlick(VdotH, F0);
    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3  spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    float sheen = pow(1.0 - NdotV, 4.0) * 0.06;

    albedo *= (1.0 + grain - 0.06 + wear * 0.05);
    col += ambient * albedo;
    col += NdotL_wrap * (albedo * (1.0 - F)) + spec * 0.4 + vec3(sheen);
  }

  // final clamp and alpha
  col = saturate(col);
  FragColor = vec4(col, u_alpha);
}
