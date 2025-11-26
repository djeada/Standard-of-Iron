#version 330 core

in vec3 v_normal;
in vec3 v_worldNormal;
in vec3 v_tangent;
in vec3 v_bitangent;
in vec2 v_texCoord;
in vec3 v_worldPos;
in float v_armorLayer;
in float v_bodyHeight;
in float v_layerNoise;
in float v_plateStress;
in float v_lamellaPhase;
in float v_latitudeMix;
in float v_cuirassProfile;
in float v_chainmailMix;
in float v_frontMask;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_useTexture;
uniform float u_alpha;
uniform int u_materialId;

out vec4 FragColor;

const vec3 k_bronze_base = vec3(0.60, 0.42, 0.18);
const vec3 k_linen_base = vec3(0.88, 0.82, 0.72);
const vec3 k_leather_base = vec3(0.38, 0.25, 0.15);
const float k_pi = 3.14159265;

float hash21(vec2 p) {
  p = fract(p * vec2(234.34, 435.345));
  p += dot(p, p + 34.45);
  return fract(p.x * p.y);
}

float noise3(vec3 p) {
  vec3 i = floor(p);
  vec3 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float n = i.x + i.y * 57.0 + i.z * 113.0;
  return mix(
      mix(mix(hash21(vec2(n, n + 1.0)), hash21(vec2(n + 57.0, n + 58.0)), f.x),
          mix(hash21(vec2(n + 113.0, n + 114.0)),
              hash21(vec2(n + 170.0, n + 171.0)), f.x),
          f.y),
      mix(mix(hash21(vec2(n + 226.0, n + 227.0)),
              hash21(vec2(n + 283.0, n + 284.0)), f.x),
          mix(hash21(vec2(n + 339.0, n + 340.0)),
              hash21(vec2(n + 396.0, n + 397.0)), f.x),
          f.y),
      f.z);
}

float fbm(vec3 p) {
  float value = 0.0;
  float amp = 0.5;
  float freq = 1.0;
  for (int i = 0; i < 5; ++i) {
    value += amp * noise3(p * freq);
    freq *= 1.85;
    amp *= 0.55;
  }
  return value;
}

vec3 hemi_ambient(vec3 n) {
  float up = clamp(n.y * 0.5 + 0.5, 0.0, 1.0);
  vec3 sky = vec3(0.57, 0.66, 0.78);
  vec3 ground = vec3(0.32, 0.26, 0.20);
  return mix(ground, sky, up);
}

float D_GGX(float NdotH, float a) {
  float a2 = a * a;
  float d = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
  return a2 / max(3.14159265 * d * d, 1e-6);
}

float geometry_schlick_ggx(float NdotX, float k) {
  return NdotX / max(NdotX * (1.0 - k) + k, 1e-6);
}

float geometry_smith(float NdotV, float NdotL, float roughness) {
  float r = roughness + 1.0;
  float k = (r * r) / 8.0;
  return geometry_schlick_ggx(NdotV, k) * geometry_schlick_ggx(NdotL, k);
}

vec3 fresnel_schlick(float cos_theta, vec3 F0) {
  return F0 + (1.0 - F0) * pow(1.0 - cos_theta, 5.0);
}

vec3 perturb(vec3 N, vec3 T, vec3 B, vec3 amount) {
  return normalize(N + T * amount.x + B * amount.y);
}

struct MaterialSample {
  vec3 color;
  vec3 normal;
  float roughness;
  vec3 F0;
};

MaterialSample sample_hammered_bronze(vec3 base_color, vec3 pos, vec3 N, vec3 T,
                                      vec3 B) {
  MaterialSample m;
  float hammer = fbm(pos * 12.0);
  float patina = fbm(pos * 5.0 + vec3(2.7, 0.3, 5.5));
  vec3 Np =
      perturb(N, T, B, vec3((hammer - 0.5) * 0.12, (patina - 0.5) * 0.07, 0.0));

  vec3 tint = mix(k_bronze_base, base_color, 0.35);
  tint = mix(tint, vec3(0.20, 0.47, 0.40), clamp(patina * 0.55, 0.0, 0.6));
  tint += vec3(0.05) * pow(max(dot(Np, vec3(0.0, 1.0, 0.1)), 0.0), 5.0);

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.3 + hammer * 0.25 + patina * 0.18, 0.18, 0.72);
  m.F0 = mix(vec3(0.06), vec3(0.92, 0.66, 0.46), 0.9);
  return m;
}

MaterialSample sample_muscle_bronze(vec3 base_color, vec3 pos, vec3 N, vec3 T,
                                    vec3 B, float sculpt, float front_mask) {
  MaterialSample m;
  float hammer = fbm(pos * 9.0);
  float profile = sculpt * 2.0 - 1.0;
  float chest_line = sin(pos.x * 6.0 + profile * 3.5);
  float ab_divide = sin(pos.y * 11.0 - profile * 4.0);
  vec3 Np = perturb(N, T, B,
                    vec3((chest_line + profile * 0.6) * 0.06,
                         (ab_divide + front_mask * 0.4) * 0.05, 0.0));

  vec3 tint = mix(k_bronze_base, base_color, 0.65);
  tint = mix(tint, tint * vec3(1.05, 0.98, 0.90),
             smoothstep(-0.2, 0.8, profile) * 0.35);
  tint += vec3(0.08) * front_mask * smoothstep(0.45, 0.95, sculpt);
  tint -= vec3(0.04) * hammer;

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.28 + hammer * 0.25 - front_mask * 0.05, 0.16, 0.70);
  m.F0 = mix(vec3(0.08), vec3(0.94, 0.68, 0.44), 0.85);
  return m;
}

MaterialSample sample_chainmail(vec3 base_color, vec3 pos, vec3 N, vec3 T,
                                vec3 B, float band_mix) {
  MaterialSample m;
  vec2 uv = pos.xz * 12.0 + pos.yx * 6.0;
  float ring_a = sin(uv.x) * cos(uv.y);
  float ring_b = sin((uv.x + uv.y) * 0.5);
  float ring_pattern = mix(ring_a, ring_b, 0.5);
  float weave = fbm(vec3(uv, 0.0) * 0.6 + v_layerNoise);
  vec3 Np = perturb(
      N, T, B, vec3((ring_pattern - 0.5) * 0.05, (band_mix - 0.5) * 0.04, 0.0));

  vec3 tint = mix(vec3(0.46, 0.48, 0.53), base_color, 0.3);
  tint *= 1.0 - weave * 0.12;
  tint += vec3(0.05) * smoothstep(0.4, 0.9, band_mix);

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.42 - ring_pattern * 0.08 + weave * 0.18, 0.2, 0.85);
  m.F0 = vec3(0.14);
  return m;
}

vec3 crest_basis(vec3 n) {
  float az = atan(n.z, n.x);
  float el = acos(clamp(n.y, -1.0, 1.0));
  return vec3(az / (2.0 * k_pi), el / k_pi, n.y);
}

MaterialSample sample_carthage_helmet(vec3 base_color, vec3 pos, vec3 N, vec3 T,
                                      vec3 B) {
  MaterialSample m;

  float hammer = fbm(pos * 14.0 + vec3(v_layerNoise));
  float patina = fbm(pos * vec3(4.2, 6.0, 4.8) + vec3(1.3, 0.0, 2.1));
  vec3 Np =
      perturb(N, T, B, vec3((hammer - 0.5) * 0.10, (patina - 0.5) * 0.14, 0.0));

  vec3 uv = crest_basis(N);
  float meridian = smoothstep(0.23, 0.0, abs(sin(uv.x * k_pi * 5.5)));
  float ridge = smoothstep(0.10, 0.0, abs(uv.x - 0.5));
  float crest = smoothstep(0.34, 0.16, uv.y) * ridge;
  float rim = smoothstep(0.80, 0.62, uv.y);
  float tip = smoothstep(0.82, 0.98, uv.y);
  vec3 rib_normal = vec3(0.0, crest * 0.42 + rim * 0.18, meridian * 0.26);

  float brush = sin(dot(T.xz, vec2(62.0, 54.0)) + uv.x * 18.0 + uv.y * 7.0);
  float scratch = smoothstep(0.6, 1.0, abs(brush));
  vec3 scratch_normal = T * (scratch * 0.10);
  Np = normalize(Np + T * rib_normal.z + B * rib_normal.y + scratch_normal);

  float brow = smoothstep(0.18, 0.0, abs(uv.y - 0.70));
  float patina_mix = clamp(patina * 0.65 + brow * 0.3 + rim * 0.25, 0.0, 1.0);

  vec3 tint = mix(vec3(0.0, 1.0, 0.0), base_color, 0.15); // debug extreme green
  vec3 patina_color = vec3(0.26, 0.52, 0.40);
  vec3 crest_highlight = vec3(1.0, 0.92, 0.75);

  tint = mix(tint, patina_color, patina_mix * 0.7);
  tint += crest_highlight * crest * 0.65;
  tint = mix(tint, tint * vec3(1.06, 1.02, 0.96), rim * 0.35);
  tint = mix(tint, tint * vec3(1.12, 1.06, 1.00), tip * 0.45);
  tint += vec3(0.14) * brow;
  float edgeWear = clamp(rim * 0.6 + tip * 0.4, 0.0, 1.0);
  tint = mix(tint, tint * vec3(1.25, 1.18, 1.05), edgeWear);
  float underside = smoothstep(0.55, 0.28, uv.y);
  tint = mix(tint, tint * vec3(0.75, 0.70, 0.66), underside * 0.6);

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.22 + hammer * 0.26 + patina * 0.18 - crest * 0.12 -
                          rim * 0.06 - tip * 0.08,
                      0.10, 0.60);
  m.F0 = mix(vec3(0.08), vec3(0.96, 0.82, 0.56),
             clamp(0.48 + crest * 0.4 + brow * 0.18 + rim * 0.12 + tip * 0.16,
                   0.0, 1.0));
  return m;
}

MaterialSample sample_lamellar_linen(vec3 base_color, vec3 pos, vec3 N, vec3 T,
                                     vec3 B) {
  MaterialSample m;
  float slat = sin(pos.x * 32.0 + v_plateStress * 1.5);
  float seam = v_lamellaPhase;
  float edge = smoothstep(0.92, 1.0, seam) + smoothstep(0.0, 0.08, seam);
  float weave = fbm(pos * 6.0 + vec3(v_layerNoise));
  vec3 Np = perturb(N, T, B, vec3(slat * 0.04, weave * 0.03, 0.0));

  vec3 tint = mix(k_linen_base, base_color, 0.4);
  tint *= 1.0 - 0.12 * weave;
  tint = mix(tint, tint * vec3(0.82, 0.76, 0.70),
             smoothstep(0.55, 1.0, v_bodyHeight));

  float plate_highlight = edge * 0.10;
  float rivet_noise = smoothstep(0.85, 1.0, seam) *
                      hash21(vec2(pos.x * 9.0, pos.y * 7.0)) * 0.04;

  m.color = tint + vec3(plate_highlight + rivet_noise);
  m.normal = Np;
  m.roughness = clamp(0.62 + weave * 0.18 - edge * 0.1, 0.35, 0.9);
  m.F0 = vec3(0.028);
  return m;
}

MaterialSample sample_dyed_leather(vec3 base_color, vec3 pos, vec3 N, vec3 T,
                                   vec3 B) {
  MaterialSample m;
  float grain = fbm(pos * 4.0);
  float crack = fbm(pos * 9.0 + vec3(0.0, 1.7, 2.3));
  vec3 Np =
      perturb(N, T, B, vec3((grain - 0.5) * 0.08, (crack - 0.5) * 0.06, 0.0));

  vec3 tint = mix(k_leather_base, base_color, 0.5);
  tint *= 1.0 - 0.12 * grain;
  tint += vec3(0.05) * smoothstep(0.4, 0.9, crack);

  m.color = tint;
  m.normal = Np;
  m.roughness = clamp(0.55 + grain * 0.25 - crack * 0.18, 0.25, 0.95);
  m.F0 = vec3(0.035);
  return m;
}

void main() {
  vec3 base_color = u_color;
  if (u_useTexture) {
    base_color *= texture(u_texture, v_texCoord).rgb;
  }

  // Material ID: 0=body/skin, 1=armor, 2=helmet, 3=weapon, 4=shield
  bool is_armor = (u_materialId == 1);
  bool is_helmet = (u_materialId == 2);
  bool is_weapon = (u_materialId == 3);
  bool is_shield = (u_materialId == 4);

  // Use material IDs exclusively (no fallbacks)
  bool helmet_region = is_helmet;
  bool torso_region = is_armor;

  vec3 Nw = normalize(v_worldNormal);
  vec3 Tw = normalize(v_tangent);
  vec3 Bw = normalize(v_bitangent);

  MaterialSample mat;
  if (helmet_region) {
    mat = sample_carthage_helmet(base_color, v_worldPos, Nw, Tw, Bw);
  } else if (torso_region) {
    MaterialSample bronze = sample_muscle_bronze(
        base_color, v_worldPos, Nw, Tw, Bw, v_cuirassProfile, v_frontMask);
    MaterialSample mail = sample_chainmail(
        base_color, v_worldPos * 1.1, Nw, Tw, Bw,
        clamp(v_chainmailMix + (1.0 - v_frontMask) * 0.25, 0.0, 1.0));
    float mail_blend = smoothstep(0.3, 0.85, v_chainmailMix) *
                       smoothstep(0.1, 0.85, 1.0 - v_frontMask);
    float bronze_blend = 1.0 - mail_blend;
    mat.color = bronze.color * bronze_blend + mail.color * mail_blend;
    mat.normal =
        normalize(bronze.normal * bronze_blend + mail.normal * mail_blend);
    mat.roughness = mix(bronze.roughness, mail.roughness, mail_blend);
    mat.F0 = mix(bronze.F0, mail.F0, mail_blend);
  } else {
    MaterialSample greave =
        sample_hammered_bronze(base_color, v_worldPos * 1.2, Nw, Tw, Bw);
    MaterialSample skirt_mail =
        sample_chainmail(base_color, v_worldPos * 0.9, Nw, Tw, Bw,
                         clamp(v_chainmailMix + 0.35, 0.0, 1.0));
    MaterialSample skirt_leather =
        sample_dyed_leather(base_color, v_worldPos * 0.8, Nw, Tw, Bw);
    float mail_bias = smoothstep(0.25, 0.75, v_chainmailMix);
    float bronze_mix = smoothstep(0.2, 0.6, v_layerNoise);
    float skirt_blend = mix(mail_bias, bronze_mix, 0.4);
    float mail_weight = clamp(skirt_blend, 0.0, 1.0);
    mat.color = mix(skirt_leather.color, greave.color, bronze_mix);
    mat.color = mix(mat.color, skirt_mail.color, mail_weight * 0.7);
    mat.normal =
        normalize(mix(skirt_leather.normal, greave.normal, bronze_mix));
    mat.normal =
        normalize(mix(mat.normal, skirt_mail.normal, mail_weight * 0.7));
    mat.roughness = mix(skirt_leather.roughness, greave.roughness, bronze_mix);
    mat.roughness = mix(mat.roughness, skirt_mail.roughness, mail_weight * 0.7);
    mat.F0 = mix(skirt_leather.F0, greave.F0, bronze_mix);
    mat.F0 = mix(mat.F0, skirt_mail.F0, mail_weight * 0.7);
  }

  vec3 L = normalize(vec3(0.45, 1.12, 0.35));
  vec3 V = normalize(vec3(0.0, 0.0, 1.0));
  vec3 H = normalize(L + V);

  float NdotL = max(dot(mat.normal, L), 0.0);
  float NdotV = max(dot(mat.normal, V), 0.0);
  float NdotH = max(dot(mat.normal, H), 0.0);
  float VdotH = max(dot(V, H), 0.0);

  float wrap = helmet_region ? 0.15 : (torso_region ? 0.28 : 0.32);
  float diff = max(NdotL * (1.0 - wrap) + wrap, 0.18);

  float a = max(0.01, mat.roughness * mat.roughness);
  float D = D_GGX(NdotH, a);
  float G = geometry_smith(NdotV, NdotL, mat.roughness);
  vec3 F = fresnel_schlick(VdotH, mat.F0);
  vec3 spec = (D * G * F) / max(4.0 * NdotL * NdotV + 1e-5, 1e-5);

  float kd = 1.0 - max(max(F.r, F.g), F.b);
  if (helmet_region) {
    kd *= 0.25;
  }

  vec3 ambient = hemi_ambient(mat.normal);
  vec3 color = mat.color;

  float grime = fbm(v_worldPos * vec3(2.0, 1.4, 2.3));
  color =
      mix(color, color * vec3(0.78, 0.72, 0.68), smoothstep(0.45, 0.9, grime));

  vec3 lighting =
      ambient * color * 0.55 + color * kd * diff + spec * max(NdotL, 0.0);

  FragColor = vec4(clamp(lighting, 0.0, 1.0), u_alpha);
}
