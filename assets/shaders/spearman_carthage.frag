#version 330 core

// === Inputs preserved (do not change) ===
in vec3 v_normal;
in vec2 v_texCoord;
in vec3 v_worldPos;
in float v_armorLayer; // Armor layer from vertex shader

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_useTexture;
uniform float u_alpha;

out vec4 FragColor;

// === Utility ===
float saturate(float x) { return clamp(x, 0.0, 1.0); }
vec3 saturate(vec3 v) { return clamp(v, 0.0, 1.0); }

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

// Fixed bug: use 2D input (was referencing p.z).
float fabricWeave(vec2 p) {
  float weaveU = sin(p.x * 60.0);
  float weaveV = sin(p.y * 60.0);
  return weaveU * weaveV * 0.05;
}

// Hemispheric ambient (simple IBL feel without extra uniforms)
vec3 hemiAmbient(vec3 n) {
  float up = saturate(n.y * 0.5 + 0.5);
  vec3 sky = vec3(0.46, 0.66, 0.78) * 0.36;
  vec3 ground = vec3(0.18, 0.16, 0.14) * 0.28;
  return mix(ground, sky, up);
}

// Schlick Fresnel
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
  return F0 + (1.0 - F0) * pow(1.0 - saturate(cosTheta), 5.0);
}

// GGX / Trowbridge-Reitz
float distributionGGX(float NdotH, float a) {
  float a2 = a * a;
  float d = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
  return a2 / max(3.14159265 * d * d, 1e-6);
}

// Smith's Schlick-G for GGX
float geometrySchlickGGX(float NdotX, float k) {
  return NdotX / max(NdotX * (1.0 - k) + k, 1e-6);
}
float geometrySmith(float NdotV, float NdotL, float roughness) {
  float r = roughness + 1.0;
  float k = (r * r) / 8.0; // Schlick approximation
  float ggx1 = geometrySchlickGGX(NdotV, k);
  float ggx2 = geometrySchlickGGX(NdotL, k);
  return ggx1 * ggx2;
}

// Screen-space curvature (edge detector) from normal derivatives
float edgeWearMask(vec3 n) {
  vec3 nx = dFdx(n);
  vec3 ny = dFdy(n);
  float curvature = length(nx) + length(ny);
  return saturate(smoothstep(0.10, 0.70, curvature));
}

// Build an approximate TBN from derivatives (no new inputs needed)
void buildTBN(out vec3 T, out vec3 B, out vec3 N, vec3 n, vec3 pos, vec2 uv) {
  vec3 dp1 = dFdx(pos);
  vec3 dp2 = dFdy(pos);
  vec2 duv1 = dFdx(uv);
  vec2 duv2 = dFdy(uv);

  float det = duv1.x * duv2.y - duv1.y * duv2.x;
  vec3 t = (dp1 * duv2.y - dp2 * duv1.y) * (det == 0.0 ? 1.0 : sign(det));
  T = normalize(t - n * dot(n, t));
  B = normalize(cross(n, T));
  N = normalize(n);
}

// Cheap bump from a procedural height map in UV space
vec3 perturbNormalFromHeight(vec3 n, vec3 pos, vec2 uv, float height,
                             float scale, float strength) {
  vec3 T, B, N;
  buildTBN(T, B, N, n, pos, uv);

  // Finite-difference heights in UV for gradient
  float h0 = height;
  float hx = noise((uv + vec2(0.002, 0.0)) * scale) - h0;
  float hy = noise((uv + vec2(0.0, 0.002)) * scale) - h0;

  vec3 bump = normalize(N + (T * hx + B * hy) * strength);
  return bump;
}

void main() {
  // Base color
  vec3 color = u_color;
  if (u_useTexture) {
    color *= texture(u_texture, v_texCoord).rgb;
  }

  // Inputs & coordinate prep
  vec3 N = normalize(v_normal);
  vec2 uvW = v_worldPos.xz * 4.5;
  vec2 uv = v_texCoord * 4.5;

  float avgColor = (color.r + color.g + color.b) / 3.0;
  float colorHue =
      max(max(color.r, color.g), color.b) - min(min(color.r, color.g), color.b);

  // Material classification preserved
  bool isMetal = (avgColor > 0.45 && avgColor <= 0.65 && colorHue < 0.15);
  bool isLeather = (avgColor > 0.30 && avgColor <= 0.50 && colorHue < 0.20);
  bool isFabric = (avgColor > 0.25 && !isMetal && !isLeather);

  // Lighting basis (kept compatible with prior shader)
  vec3 L = normalize(vec3(1.0, 1.15, 1.0));
  // Approximate view vector from world origin; nudged to avoid degenerate
  // normalization
  vec3 V = normalize(-v_worldPos + N * 0.001);
  vec3 H = normalize(L + V);

  float NdotL = saturate(dot(N, L));
  float NdotV = saturate(dot(N, V));
  float NdotH = saturate(dot(N, H));
  float VdotH = saturate(dot(V, H));

  // Ambient
  vec3 ambient = hemiAmbient(N);

  // Shared wrap diffuse (preserved behavior, slight tweak via saturate)
  float wrapAmount = isMetal ? 0.14 : (isLeather ? 0.28 : 0.38);
  float diffWrap = max(NdotL * (1.0 - wrapAmount) + wrapAmount, 0.20);
  if (isMetal)
    diffWrap = pow(diffWrap, 0.88);

  // Edge & cavity masks (for wear/rust/shine)
  float edgeMask = edgeWearMask(N);  // bright edges
  float cavityMask = 1.0 - edgeMask; // crevices
  // Gravity bias: downward-facing areas collect more dirt/rust
  float downBias = saturate((-N.y) * 0.6 + 0.4);
  cavityMask *= downBias;

  // === Material models ===
  vec3 F0 = vec3(0.045, 0.05, 0.055);
  float roughness = 0.6; // default roughness
  float cavityAO = 1.0;  // occlusion multiplier
  vec3 albedo = color;   // base diffuse/albedo
  vec3 specular = vec3(0.0);

  if (isMetal) {
    // Use texture UVs for stability (as in original)
    vec2 metalUV = v_texCoord * 4.5;

    // Brushed/anisotropic micro-lines & microdents
    float brushed = abs(sin(v_texCoord.y * 85.0)) * 0.022;
    float dents = noise(metalUV * 6.0) * 0.035;
    float rustTex = noise(metalUV * 8.0) * 0.10;

    // Small directional scratches
    float scratchLines = smoothstep(
        0.97, 1.0, abs(sin(metalUV.x * 160.0 + noise(metalUV * 3.0) * 2.0)));
    scratchLines *= 0.08;

    // Procedural height for bumping (kept subtle to avoid shimmer)
    float height = noise(metalUV * 12.0) * 0.5 + brushed * 2.0 + scratchLines;
    vec3 Np =
        perturbNormalFromHeight(N, v_worldPos, v_texCoord, height, 12.0, 0.55);
    N = mix(N, Np, 0.65); // blend to keep stable

    // Physically-based specular with GGX
    roughness = clamp(0.18 + brushed * 0.35 + dents * 0.25 + rustTex * 0.30 -
                          edgeMask * 0.12,
                      0.05, 0.9);
    float a = max(0.001, roughness * roughness);

    // Metals take F0 from their base color
    F0 = saturate(color);

    // Rust/dirt reduce albedo and boost roughness in cavities
    float rustMask = smoothstep(0.55, 0.85, noise(metalUV * 5.5)) * cavityMask;
    vec3 rustTint = vec3(0.35, 0.18, 0.08); // warm iron-oxide tint
    albedo = mix(albedo, albedo * 0.55 + rustTint * 0.35, rustMask);
    roughness = clamp(mix(roughness, 0.85, rustMask), 0.05, 0.95);

    // Edge wear: brighten edges with lower roughness (polished)
    albedo = mix(albedo, albedo * 1.12 + vec3(0.05), edgeMask * 0.7);
    roughness = clamp(mix(roughness, 0.10, edgeMask * 0.5), 0.05, 0.95);

    // Recompute lighting terms with updated normal
    H = normalize(L + V);
    NdotL = saturate(dot(N, L));
    NdotV = saturate(dot(N, V));
    NdotH = saturate(dot(N, H));
    VdotH = saturate(dot(V, H));

    float D = distributionGGX(NdotH, a);
    float G = geometrySmith(NdotV, NdotL, roughness);
    vec3 F = fresnelSchlick(VdotH, F0);

    specular = (D * G * F) / max(4.0 * NdotL * NdotV, 1e-4);

    // Clearcoat sparkle (very subtle tight lobe)
    float aCoat = 0.04; // ~roughness 0.2
    float Dcoat = distributionGGX(NdotH, aCoat);
    float Gcoat = geometrySmith(NdotV, NdotL, sqrt(aCoat));
    vec3 Fcoat = fresnelSchlick(VdotH, vec3(0.04));
    specular += 0.06 * (Dcoat * Gcoat * Fcoat) / max(4.0 * NdotL * NdotV, 1e-4);

    // Metals have almost no diffuse term
    float kD = 0.0;
    vec3 diffuse = vec3(kD);

    // AO from cavities
    cavityAO = 1.0 - rustMask * 0.6;

    // Final combine (ambient + wrapped diffuse + specular)
    vec3 lit = ambient * albedo * cavityAO + diffWrap * albedo * diffuse +
               specular * NdotL;

    // Small addition of brushed sheen from the original
    lit += vec3(brushed) * 0.8;

    color = lit;

  } else if (isLeather) {
    // Leather microstructure & wear
    float leather = leatherGrain(uvW);
    float wear = noise(uvW * 4.0) * 0.12 - 0.06;

    float viewAngle = abs(dot(N, normalize(vec3(0.0, 1.0, 0.5))));
    float leatherSheen = pow(1.0 - viewAngle, 5.0) * 0.12;

    albedo *= 1.0 + leather - 0.08 + wear;
    albedo += vec3(leatherSheen);

    // Leather: dielectric
    roughness = clamp(0.55 + leather * 0.25, 0.2, 0.95);
    float a = roughness * roughness;

    float D = distributionGGX(NdotH, a);
    float G = geometrySmith(NdotV, NdotL, roughness);
    vec3 F = fresnelSchlick(VdotH, F0);
    specular = (D * G * F) / max(4.0 * NdotL * NdotV, 1e-4);

    float kD = 1.0 - max(max(F.r, F.g), F.b);
    vec3 diffuse = kD * albedo;

    cavityAO = 1.0 - (noise(uvW * 3.0) * 0.15) * cavityMask;

    color = ambient * albedo * cavityAO + diffWrap * diffuse + specular * NdotL;

  } else if (isFabric) {
    float weave = fabricWeave(v_worldPos.xz);
    float fabricFuzz = noise(uvW * 18.0) * 0.08;
    float folds = noise(uvW * 5.0) * 0.10 - 0.05;

    float viewAngle = abs(dot(N, normalize(vec3(0.0, 1.0, 0.5))));
    float fabricSheen = pow(1.0 - viewAngle, 7.0) * 0.10;

    albedo *= 1.0 + fabricFuzz - 0.04 + folds;
    albedo += vec3(weave + fabricSheen);

    roughness = clamp(0.65 + fabricFuzz * 0.25, 0.3, 0.98);
    float a = roughness * roughness;

    float D = distributionGGX(NdotH, a);
    float G = geometrySmith(NdotV, NdotL, roughness);
    vec3 F = fresnelSchlick(VdotH, F0);
    specular = (D * G * F) / max(4.0 * NdotL * NdotV, 1e-4);

    float kD = 1.0 - max(max(F.r, F.g), F.b);
    vec3 diffuse = kD * albedo;

    cavityAO = 1.0 - (noise(uvW * 2.5) * 0.10) * cavityMask;

    color = ambient * albedo * cavityAO + diffWrap * diffuse + specular * NdotL;

  } else {
    // Generic matte
    float detail = noise(uvW * 8.0) * 0.14;
    albedo *= 1.0 + detail - 0.07;

    roughness = 0.7;
    float a = roughness * roughness;

    float D = distributionGGX(NdotH, a);
    float G = geometrySmith(NdotV, NdotL, roughness);
    vec3 F = fresnelSchlick(VdotH, F0);
    specular = (D * G * F) / max(4.0 * NdotL * NdotV, 1e-4);

    float kD = 1.0 - max(max(F.r, F.g), F.b);
    vec3 diffuse = kD * albedo;

    color = ambient * albedo + diffWrap * diffuse + specular * NdotL;
  }

  color = mix(color, vec3(0.30, 0.55, 0.65), edgeMask * 0.18);

  // Final color clamp and alpha preserved
  color = saturate(color);
  FragColor = vec4(color, u_alpha);
}
