#version 330 core
out vec4 FragColor;
in vec2 TexCoord;
in vec3 WorldPos;

uniform float time;

// ------------------------------------------------------------
const float PI = 3.14159265359;
float saturate(float x) { return clamp(x, 0.0, 1.0); }
vec3 saturate(vec3 v) { return clamp(v, vec3(0.0), vec3(1.0)); }
mat2 rot(float a) {
  float c = cos(a), s = sin(a);
  return mat2(c, -s, s, c);
}

float hash(vec2 p) {
  p = fract(p * vec2(123.34, 456.21));
  p += dot(p, p + 45.32);
  return fract(p.x * p.y);
}
float noise(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float a = hash(i), b = hash(i + vec2(1, 0)), c = hash(i + vec2(0, 1)),
        d = hash(i + vec2(1, 1));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
float fbm(vec2 p) {
  float v = 0., a = .5, f = 1.;
  for (int i = 0; i < 5; i++) {
    v += a * noise(p * f);
    f *= 2.;
    a *= .5;
  }
  return v;
}

vec3 skyColor(vec3 rd, vec3 sunDir) {
  float t = saturate(rd.y * 0.5 + 0.5);
  vec3 horizon = vec3(0.68, 0.84, 0.95), zenith = vec3(0.15, 0.36, 0.70);
  vec3 sky = mix(horizon, zenith, t);
  float sun = pow(max(dot(rd, sunDir), 0.0), 260.0);
  float halo = pow(max(dot(rd, sunDir), 0.0), 6.0) * 0.03;
  return sky + vec3(1.0, 0.96, 0.88) * (sun * 1.0 + halo);
}
float fresnelSchlick(float c, float F0) {
  return F0 + (1.0 - F0) * pow(1.0 - c, 5.0);
}
float ggxSpec(vec3 N, vec3 V, vec3 L, float rough, float F0) {
  vec3 H = normalize(V + L);
  float NdotV = max(dot(N, V), 0.0), NdotL = max(dot(N, L), 0.0);
  float NdotH = max(dot(N, H), 0.0), VdotH = max(dot(V, H), 0.0);
  float a = max(rough * rough, 0.001), a2 = a * a;
  float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
  float D = a2 / (PI * denom * denom);
  float k = (a + 1.0);
  k = (k * k) / 8.0;
  float Gv = NdotV / (NdotV * (1.0 - k) + k),
        Gl = NdotL / (NdotL * (1.0 - k) + k);
  float F = fresnelSchlick(VdotH, F0);
  return (D * Gv * Gl * F) / max(4.0 * NdotV * NdotL, 0.001);
}

// ---------------- water field (isotropic, world-space) ---------------
float waterHeight(vec2 uv) {
  vec2 p = uv;
  vec2 w1 = vec2(fbm(p * 0.6 + time * 0.05), fbm(p * 0.6 - time * 0.04));
  vec2 w2 = vec2(fbm(rot(1.3) * p * 0.9 - time * 0.03),
                 fbm(rot(2.1) * p * 0.7 + time * 0.02));
  p += 0.75 * w1 + 0.45 * w2;
  float h = 0.0;
  h += 0.55 * (fbm(p * 1.6 - time * 0.15) - 0.5);
  h += 0.30 * (fbm(rot(0.8) * p * 2.8 + time * 0.20) - 0.5);
  h += 0.15 * (fbm(rot(2.4) * p * 5.0 - time * 0.35) - 0.5);
  return h;
}

void heightDeriv(vec2 uv, out float h, out vec2 grad, out float lap) {
  float s = max(0.003, 0.35 * length(fwidth(uv)));
  vec2 e = vec2(s);
  float hc = waterHeight(uv);
  float hx1 = waterHeight(uv + vec2(e.x, 0));
  float hx2 = waterHeight(uv - vec2(e.x, 0));
  float hy1 = waterHeight(uv + vec2(0, e.y));
  float hy2 = waterHeight(uv - vec2(0, e.y));
  grad = vec2((hx1 - hx2) / (2.0 * e.x), (hy1 - hy2) / (2.0 * e.y)) * 0.85;
  lap = (hx1 + hx2 + hy1 + hy2 - 4.0 * hc) / (e.x * e.x + e.y * e.y);
  h = hc;
}

vec3 microNormal(vec2 uv) {
  float s = 7.0;
  vec2 e = vec2(max(0.0015, 0.5 * length(fwidth(uv))));
  float mx1 = fbm(uv * s + time * 0.6 + vec2(e.x, 0)),
        mx2 = fbm(uv * s + time * 0.6 - vec2(e.x, 0));
  float my1 = fbm(uv * s + time * 0.6 + vec2(0, e.y)),
        my2 = fbm(uv * s + time * 0.6 - vec2(0, e.y));
  vec2 g = vec2((mx1 - mx2) / (2.0 * e.x), (my1 - my2) / (2.0 * e.y));
  return normalize(vec3(-g.x, 0.35, -g.y));
}
vec3 waterNormal(vec2 uv, vec2 grad) {
  float k = 3.2;
  vec3 N = normalize(vec3(-grad.x * k, 1.0, -grad.y * k));
  return normalize(
      mix(N, normalize(N + 0.22 * (microNormal(uv) - vec3(0, 1, 0))), 0.35));
}

// ---------------- main ----------------
void main() {
  // world-space UV so no seams
  vec2 uv = rot(0.35) * (WorldPos.xz * 0.38);

  float h, lap;
  vec2 grad;
  heightDeriv(uv, h, grad, lap);
  vec3 N = waterNormal(uv, grad);

  vec3 sunDir = normalize(vec3(0.28, 0.85, 0.43));
  vec3 V = normalize(vec3(0.0, 0.7, 0.7));

  float NdotV = max(dot(N, V), 0.0);
  float F0 = 0.02;

  // ---------- transmission (make it BLUER) ----------
  vec3 deepWater = vec3(0.008, 0.035, 0.080); // deeper, bluer
  vec3 shallowWater = vec3(0.060, 0.180, 0.300);

  float calm = smoothstep(0.0, 0.45, abs(h));
  float shallow = saturate(0.35 + 0.35 * (fbm(uv * 0.6) * (1.0 - calm)));

  // stronger red/green absorption so blue survives
  vec3 absorb = vec3(0.90, 0.45, 0.12);
  float thickness =
      mix(0.6, 3.5, 1.0 - shallow) * (0.35 + pow(1.0 - NdotV, 0.7));
  vec3 transBase = mix(deepWater, shallowWater, shallow);
  vec3 transmission = transBase * exp(-absorb * thickness);

  // ---------- reflection (dim + blue-tinted) ----------
  vec3 R = reflect(-V, N);
  vec3 reflection = skyColor(R, sunDir);
  reflection *= 0.70;                         // dim
  reflection *= vec3(0.60, 0.75, 1.00);       // blue tint
  float F = fresnelSchlick(NdotV, F0) * 0.40; // less mirror

  // ---------- lighting (tamed) ----------
  float NdotL = max(dot(N, sunDir), 0.0);
  float rough = mix(0.12, 0.26, smoothstep(0.0, 0.6, length(grad)));
  float spec = ggxSpec(N, V, sunDir, rough, F0) * 0.50; // half energy
  vec3 specCol = vec3(0.75, 0.85, 1.10) * spec;         // watery tint
  vec3 sunDiffuse = transmission * NdotL * 0.20;

  // shoreline foam only (softer & less white)
  float shore = 1.0 - (smoothstep(0.07, 0.28, TexCoord.y) *
                       smoothstep(0.07, 0.28, 1.0 - TexCoord.y));
  float foam = shore * (0.45 + 0.55 * fbm(uv * 3.0 + time * 0.6));
  vec3 foamCol = vec3(0.92, 0.96, 1.0);
  foam = clamp(foam * 0.35, 0.0, 1.0); // much less foam

  // ---------- combine (energy aware) ----------
  vec3 color = transmission * (1.0 - F) + reflection * F; // avoid whitening
  color += specCol + sunDiffuse;
  color = mix(color, foamCol * mix(0.82, 1.0, NdotL), foam);

  // gentle blue rim
  color += vec3(0.03, 0.06, 0.12) * pow(1.0 - NdotV, 3.0);

  FragColor = vec4(saturate(color), 0.85);
}
