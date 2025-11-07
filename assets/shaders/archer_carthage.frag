#version 330 core

in vec3 v_normal;
in vec3 v_worldNormal;
in vec3 v_tangent;
in vec3 v_bitangent;
in vec2 v_texCoord;
in vec3 v_worldPos;
in float v_armorLayer;
in float v_leatherTension;
in float v_bodyHeight;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_useTexture;
uniform float u_alpha;
uniform float u_time;
uniform float u_rainIntensity;

out vec4 FragColor;

// ----------------------------- Utils & Noise -----------------------------
const float PI = 3.14159265359;

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

float triplanarNoise(vec3 pos, vec3 normal, float scale) {
  vec3 b = abs(normal);
  b = max(b, vec3(0.0001));
  b /= (b.x + b.y + b.z);
  float xy = noise(pos.xy * scale);
  float yz = noise(pos.yz * scale);
  float zx = noise(pos.zx * scale);
  return xy * b.z + yz * b.x + zx * b.y;
}

float fbm(vec3 pos, vec3 normal, float scale) {
  float total = 0.0;
  float amp = 0.5;
  float freq = 1.0;
  for (int i = 0; i < 5; ++i) {
    total += amp * triplanarNoise(pos * freq, normal, scale * freq);
    freq *= 2.02;
    amp *= 0.45;
  }
  return total;
}

// Luma / chroma helpers
float luma(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }
float sat(vec3 c) {
  float mx = max(max(c.r, c.g), c.b);
  float mn = min(min(c.r, c.g), c.b);
  return (mx - mn) / max(mx, 1e-5);
}

// ----------------------------- Geometry helpers -----------------------------
float fresnelSchlick(float cosTheta, float F0) {
  return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float computeCurvature(vec3 normal) {
  vec3 dx = dFdx(normal);
  vec3 dy = dFdy(normal);
  return length(dx) + length(dy);
}

// Microfacet (GGX) – isotropic (we’ll tint/spec weight per material)
float D_GGX(float NdotH, float a) {
  float a2 = a * a;
  float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
  return a2 / max(PI * d * d, 1e-6);
}

float G_Smith(float NdotV, float NdotL, float a) {
  // Schlick-GGX
  float k = (a + 1.0);
  k = (k * k) / 8.0;
  float gV = NdotV / (NdotV * (1.0 - k) + k);
  float gL = NdotL / (NdotL * (1.0 - k) + k);
  return gV * gL;
}

// Simple wrap diffuse (Oren–Nayar would need sigma; we keep interface)
float wrapDiffuse(vec3 normal, vec3 lightDir, float wrap) {
  float nl = dot(normal, lightDir);
  return max(nl * (1.0 - wrap) + wrap, 0.0);
}

// Perturb normal procedurally to add micro detail per material
vec3 perturbNormalLeather(vec3 N, vec3 T, vec3 B, vec3 P) {
  float g1 = fbm(P * 6.0, N, 8.0);
  float g2 = fbm(P * 18.0 + vec3(2.1), N, 24.0);
  vec3 nT = T * (g1 * 0.06 + g2 * 0.02);
  vec3 nB = B * (g1 * 0.03 + g2 * 0.03);
  vec3 pN = normalize(N + nT + nB);
  return pN;
}

vec3 perturbNormalLinen(vec3 N, vec3 T, vec3 B, vec3 P) {
  // warp/weft weave
  float warp = sin(P.x * 140.0) * 0.06;
  float weft = sin(P.z * 146.0) * 0.06;
  float slub = fbm(P * 7.0, N, 10.0) * 0.04;
  vec3 pN = normalize(N + T * (warp + slub) + B * (weft + slub * 0.5));
  return pN;
}

vec3 perturbNormalBronze(vec3 N, vec3 T, vec3 B, vec3 P) {
  float hammer = fbm(P * 16.0, N, 22.0) * 0.10;
  float ripple = fbm(P * 40.0 + vec3(3.7), N, 55.0) * 0.03;
  vec3 pN = normalize(N + T * hammer + B * ripple);
  return pN;
}

// Clearcoat (water film) lobe for rain
vec3 clearcoatSpec(vec3 N, vec3 L, vec3 V, float coatStrength,
                   float coatRough) {
  vec3 H = normalize(L + V);
  float NdotV = max(dot(N, V), 0.0);
  float NdotL = max(dot(N, L), 0.0);
  float NdotH = max(dot(N, H), 0.0);
  float a = max(coatRough, 0.02);
  float D = D_GGX(NdotH, a);
  float G = G_Smith(NdotV, NdotL, a);
  // IOR ~1.33 → F0 ≈ 0.02
  float F = fresnelSchlick(max(dot(H, V), 0.0), 0.02);
  float spec = (D * G * F) / max(4.0 * NdotV * NdotL + 1e-5, 1e-5);
  return vec3(spec * coatStrength);
}

// ----------------------------- Main -----------------------------
void main() {
  // Base color (historically-inspired calibration: leather/linen/bronze)
  vec3 baseColor = u_color;
  if (u_useTexture) {
    baseColor *= texture(u_texture, v_texCoord).rgb;
  }

  // Material classification (robust by luma/sat, keeps your heuristics spirit)
  float Y = luma(baseColor);
  float S = sat(baseColor);
  bool likelyLeather =
      (Y > 0.18 && Y < 0.65 && baseColor.r > baseColor.g * 1.03);
  bool likelyLinen = (Y > 0.65 && S < 0.22); // undyed ecru/off-white
  bool likelyBronze = (baseColor.r > baseColor.g * 1.03 &&
                       baseColor.r > baseColor.b * 1.10 && Y > 0.42);

  // Canonical normal/TBN
  vec3 Nw = normalize(v_worldNormal);
  vec3 Tw = normalize(v_tangent);
  vec3 Bw = normalize(v_bitangent);

  // Lighting (keep your single sun, plus very light ambient model)
  vec3 L = normalize(vec3(0.55, 1.15, 0.35));
  vec3 V = normalize(
      vec3(0.0, 0.0, 1.0)); // no camera pos available; preserve interface
  vec3 H = normalize(L + V);

  float rain = clamp(u_rainIntensity, 0.0, 1.0);
  float curvature = computeCurvature(Nw);

  // Sky/ground ambient: cool from above, warm bounce from below
  float up = clamp(Nw.y, 0.0, 1.0);
  float down = clamp(-Nw.y, 0.0, 1.0);
  vec3 skyAmbient = vec3(0.60, 0.70, 0.85) * (0.20 + 0.35 * up);
  vec3 groundBounce = vec3(0.40, 0.34, 0.28) * (0.06 + 0.20 * down);
  vec3 ambient = skyAmbient + groundBounce;

  // Raindrop streak mask (flows down -Y with time, gathers in hollows)
  float streak = smoothstep(0.65, 1.0,
                            sin(v_worldPos.y * 22.0 - u_time * 4.0 +
                                fbm(v_worldPos * 0.8, Nw, 1.2) * 6.283));
  float wetGather = (1.0 - clamp(Nw.y, 0.0, 1.0)) *
                    (0.4 + 0.6 * fbm(v_worldPos * 2.0, Nw, 3.0));
  float wetMask =
      clamp(rain * mix(0.5 * wetGather, 1.0 * wetGather, streak), 0.0, 1.0);

  // Start with the incoming color, we’ll push toward historically plausible
  // targets
  vec3 color = baseColor;
  vec3 N = Nw;

  // Accumulated specular
  vec3 specularAccum = vec3(0.0);

  // Common N·L/V/H
  float NdotL, NdotV, NdotH;

  // ----------------------------- Leather -----------------------------
  if (likelyLeather) {
    // Historical vegetable-tanned leather: warm brown; sun-bleached upper
    // torso, sweat/dirt lower
    vec3 targetLeather = vec3(0.42, 0.30, 0.20);
    color = mix(targetLeather, color, 0.25);

    // Subtle vertical sun-bleach gradient (upper torso brighter)
    float sunKissed = mix(0.92, 1.06, clamp(v_bodyHeight, 0.0, 1.0));
    color *= vec3(sunKissed, mix(0.90, 0.98, v_bodyHeight),
                  mix(0.87, 0.95, v_bodyHeight));

    // Procedural micro normal for grain/pores
    N = perturbNormalLeather(Nw, Tw, Bw, v_worldPos);

    // Grain/aging modulation
    float coarse = fbm(v_worldPos * 3.2, Nw, 4.0);
    float medium = fbm(v_worldPos * 7.6, Nw, 8.0);
    float fine = fbm(v_worldPos * 16.0, Nw, 20.0);
    float pores = fbm(v_worldPos * 38.0 + vec3(3.7), Nw, 48.0);
    float grain = coarse * 0.40 + medium * 0.32 + fine * 0.20 + pores * 0.08;

    // Stretch/crease based on tension and local grain
    float stretch = mix(-0.05, 0.12, clamp(v_leatherTension, 0.0, 1.0));
    float creaseWaves = sin(v_worldPos.x * 10.5 + grain * 2.1) *
                        sin(v_worldPos.z * 9.2 + grain * 2.6);
    float creaseMask = smoothstep(0.55, 0.87, creaseWaves) * 0.20;
    color = mix(color, color * 0.72, creaseMask);
    color += color * stretch * 0.5;

    // Edge wear via curvature, salt/sweat halos
    float edgeWear = smoothstep(0.34, 1.10, curvature);
    color = mix(color, color + vec3(0.14, 0.12, 0.08), edgeWear * 0.28);
    float salt = smoothstep(0.62, 0.95,
                            fbm(v_worldPos * vec3(12.0, 6.0, 12.0), Nw, 14.0));
    color = mix(color, color * vec3(0.80, 0.78, 0.75), salt * 0.12);

    // Dirt accumulation lower on body
    float dirt =
        fbm(v_worldPos * vec3(2.5, 1.1, 2.5), Nw, 3.5) * (1.0 - v_bodyHeight);
    color = mix(color, color * vec3(0.70, 0.58, 0.42),
                smoothstep(0.45, 0.75, dirt) * 0.20);

    // Wetness darkens leather and adds clearcoat
    color = mix(color, color * 0.55, wetMask * 0.85);

    // Specular: dielectric leather F0 ~ 0.035, roughness from grain/tension
    float roughness =
        clamp(0.55 - v_leatherTension * 0.18 + grain * 0.12, 0.28, 0.75);
    float a = max(0.04, roughness * roughness);
    NdotL = max(dot(N, L), 0.0);
    NdotV = max(dot(N, V), 0.0);
    NdotH = max(dot(N, normalize(L + V)), 0.0);
    float D = D_GGX(NdotH, a);
    float G = G_Smith(NdotV, NdotL, a);
    float F = fresnelSchlick(max(dot(normalize(L + V), V), 0.0), 0.035);
    float spec = (D * G * F) / max(4.0 * NdotV * NdotL + 1e-5, 1e-5);

    // Add water clearcoat when raining
    vec3 coat = clearcoatSpec(N, L, V, wetMask * 0.9, mix(0.10, 0.03, wetMask));

    specularAccum += vec3(spec) * 0.55 + coat;

    // ----------------------------- Linen -----------------------------
  } else if (likelyLinen) {
    // Undyed linen (ecru) target; historically sun-yellowing & sweat near
    // neck/upper torso
    vec3 targetLinen = vec3(0.88, 0.85, 0.78);
    color = mix(targetLinen, color, 0.35);

    // Procedural weave normal
    N = perturbNormalLinen(Nw, Tw, Bw, v_worldPos);

    // Subtle weave tint + size variation (glue/sizing + fray)
    float weave = sin(v_worldPos.x * 62.0) * sin(v_worldPos.z * 66.0) * 0.08;
    float sizing =
        fbm(v_worldPos * 3.0, Nw, 4.5) * 0.10; // starch/glue darkening
    float fray =
        fbm(v_worldPos * 9.0, Nw, 10.0) * clamp(1.4 - Nw.y, 0.0, 1.0) * 0.12;
    color += vec3(weave * 0.5);
    color -= vec3(sizing * 0.6);
    color -= vec3(fray * 0.15);

    // Dust from below, perspiration from above
    float dust = clamp(1.0 - Nw.y, 0.0, 1.0) * fbm(v_worldPos * 1.1, Nw, 2.0);
    float sweat =
        smoothstep(0.6, 1.0, v_bodyHeight) * fbm(v_worldPos * 2.4, Nw, 3.1);
    color = mix(color, color * (1.0 - dust * 0.35), 0.7);
    color = mix(color, color * vec3(0.96, 0.93, 0.88),
                1.0 - clamp(sweat * 0.5, 0.0, 1.0));

    // Damp linen darkens and gets slightly more specular
    float damp = rain * (0.18 + 0.32 * fbm(v_worldPos * 2.0, Nw, 3.5));
    color *= (1.0 - damp * 0.35);

    // Cloth specular (dielectric, low F0; slight sheen at grazing)
    NdotL = max(dot(N, L), 0.0);
    NdotV = max(dot(N, V), 0.0);
    float roughness = 0.85; // matte
    float a = roughness * roughness;
    float D = D_GGX(max(dot(N, normalize(L + V)), 0.0), a);
    float G = G_Smith(NdotV, NdotL, a);
    float F = fresnelSchlick(max(dot(normalize(L + V), V), 0.0), 0.028);
    float spec = (D * G * F) / max(4.0 * NdotV * NdotL + 1e-5, 1e-5);

    // Wet clearcoat is minimal on absorbent cloth
    vec3 coat = clearcoatSpec(N, L, V, wetMask * 0.25, 0.06);
    specularAccum += vec3(spec * 0.25) + coat;

    // ----------------------------- Bronze -----------------------------
  } else if (likelyBronze) {
    // Warm hammered bronze with cuprite (reddish) and malachite (green) patina
    vec3 bronzeWarm = vec3(0.58, 0.44, 0.20);
    vec3 cuprite = vec3(0.36, 0.18, 0.10);
    vec3 malachite = vec3(0.18, 0.45, 0.36);

    // Procedural hammered normal
    N = perturbNormalBronze(Nw, Tw, Bw, v_worldPos);

    float hammer = fbm(v_worldPos * 14.0, Nw, 20.0) * 0.18;
    float patina = fbm(v_worldPos * 6.0 + vec3(5.0), Nw, 8.0) * 0.14;
    float runOff = fbm(v_worldPos * vec3(1.2, 3.4, 1.2), Nw, 2.2) *
                   (1.0 - Nw.y); // forms under gravity

    // Color layering: base → cuprite pits → malachite edges/runoffs
    vec3 bronzeBase = mix(bronzeWarm, color, 0.35) + vec3(hammer);
    vec3 withCuprite =
        mix(bronzeBase, cuprite,
            smoothstep(0.70, 0.95, fbm(v_worldPos * 9.0, Nw, 12.0)));
    color = mix(withCuprite, malachite,
                clamp(patina * 0.5 + runOff * 0.6, 0.0, 1.0));

    // Wetness: darker diffuse, stronger sharp spec
    color = mix(color, color * 0.65, wetMask * 0.6);

    // Metallic specular (energy conserving)
    float metallic = 0.92;
    float roughness = clamp(0.25 + hammer * 0.25 + patina * 0.15, 0.15, 0.65);
    float a = roughness * roughness;

    NdotL = max(dot(N, L), 0.0);
    NdotV = max(dot(N, V), 0.0);
    NdotH = max(dot(N, normalize(L + V)), 0.0);

    float D = D_GGX(NdotH, a);
    float G = G_Smith(NdotV, NdotL, a);

    // F0 from metallic base color
    vec3 F0 = mix(vec3(0.06), clamp(bronzeWarm, 0.0, 1.0), metallic);
    float FH = max(dot(normalize(L + V), V), 0.0);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - FH, 5.0);

    vec3 spec = (D * G * F) / max(4.0 * NdotV * NdotL + 1e-5, 1e-5);
    specularAccum += spec * 0.95;

    // Clearcoat water film
    specularAccum +=
        clearcoatSpec(N, L, V, wetMask * 0.8, mix(0.10, 0.04, wetMask));

    // ----------------------------- Generic / fallback
    // -----------------------------
  } else {
    // Subtle material noise/ambient and light wetness handling
    float subtle = fbm(v_worldPos * 4.0, Nw, 5.5) * 0.12;
    color *= (1.0 + subtle);
    color = mix(color, color * 0.7, wetMask * 0.5);
    N = Nw;
    NdotL = max(dot(N, L), 0.0);
    NdotV = max(dot(N, V), 0.0);
    float roughness = 0.6;
    float a = roughness * roughness;
    float D = D_GGX(max(dot(N, normalize(L + V)), 0.0), a);
    float G = G_Smith(NdotV, NdotL, a);
    float F = fresnelSchlick(max(dot(normalize(L + V), V), 0.0), 0.04);
    float spec = (D * G * F) / max(4.0 * NdotV * NdotL + 1e-5, 1e-5);
    specularAccum += vec3(spec * 0.25);
  }

  // Diffuse (wrapped to soften terminator; bronze uses a bit tighter wrap)
  float wrap = likelyBronze ? 0.22 : (likelyLeather ? 0.35 : 0.42);
  float diff = wrapDiffuse(N, L, wrap);

  // Cavity/curvature occlusion to sit details down
  float cavity = 1.0 - smoothstep(0.2, 1.1, curvature);
  float ao = mix(0.85, 1.0, cavity);

  // Final light mix, simple energy conservation for dielectrics
  float metallicTerm = likelyBronze ? 0.92 : 0.0;
  float Favg = 0.04; // average dielectric reflectance
  float kd = (1.0 - metallicTerm) * (1.0 - Favg);

  vec3 lit = color * kd * diff + specularAccum;
  lit += ambient * color * kd * 0.5; // ambient contribution

  // Slight rain darkening in creases
  lit = mix(lit, lit * 0.85, wetMask * 0.2);

  // Clamp and out
  vec3 finalColor = clamp(lit * ao, 0.0, 1.0);
  FragColor = vec4(finalColor, u_alpha);
}
