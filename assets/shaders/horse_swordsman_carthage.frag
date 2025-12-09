#version 330 core

in vec3 v_normal;
in vec2 v_texCoord;
in vec3 v_worldPos;
in float v_armorLayer; // Armor layer from vertex shader

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_useTexture;
uniform float u_alpha;
uniform int u_materialId;

out vec4 FragColor;

// ---------------------
// utilities & noise
// ---------------------
const float PI = 3.14159265359;

float saturate(float x) { return clamp(x, 0.0, 1.0); }
vec3 saturate(vec3 x) { return clamp(x, 0.0, 1.0); }

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
  for (int i = 0; i < 5; ++i) {
    f += a * noise(p);
    p *= 2.03;
    a *= 0.5;
  }
  return f;
}

// anti-aliased step
float aa_step(float edge, float x) {
  float w = fwidth(x);
  return smoothstep(edge - w, edge + w, x);
}

// ---------------------
// patterns
// ---------------------

// plate seams + rivets (AA)
float armor_plates(vec2 p, float y) {
  float plate_y = fract(y * 6.5);
  float line = smoothstep(0.92, 0.98, plate_y) - smoothstep(0.98, 1.0, plate_y);
  // anti-aliased line thickness
  line = smoothstep(0.0, fwidth(plate_y) * 2.0, line) * 0.12;

  // rivets on top seams
  float rivet_x = fract(p.x * 18.0);
  float rivet =
      smoothstep(0.48, 0.50, rivet_x) * smoothstep(0.52, 0.50, rivet_x);
  rivet *= step(0.92, plate_y);
  return line + rivet * 0.25;
}

// linked ring suggestion (AA)
float chainmail_rings(vec2 p) {
  vec2 uv = p * 35.0;

  vec2 g0 = fract(uv) - 0.5;
  float r0 = length(g0);
  float fw0 = fwidth(r0) * 1.2;
  float ring0 = smoothstep(0.30 + fw0, 0.30 - fw0, r0) -
                smoothstep(0.20 + fw0, 0.20 - fw0, r0);

  vec2 g1 = fract(uv + vec2(0.5, 0.0)) - 0.5;
  float r1 = length(g1);
  float fw1 = fwidth(r1) * 1.2;
  float ring1 = smoothstep(0.30 + fw1, 0.30 - fw1, r1) -
                smoothstep(0.20 + fw1, 0.20 - fw1, r1);

  return (ring0 + ring1) * 0.15;
}

float horse_hide_pattern(vec2 p) {
  float grain = fbm(p * 80.0) * 0.10;
  float ripple = sin(p.x * 22.0) * cos(p.y * 28.0) * 0.035;
  float mottling = smoothstep(0.55, 0.65, fbm(p * 6.0)) * 0.07;
  return grain + ripple + mottling;
}

// ---------------------
// microfacet shading
// ---------------------
vec3 fresnel_schlick(float cos_theta, vec3 F0) {
  return F0 + (1.0 - F0) * pow(1.0 - cos_theta, 5.0);
}

float D_GGX(float NdotH, float rough) {
  float a = max(0.001, rough);
  float a2 = a * a;
  float d = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
  return a2 / max(1e-6, (PI * d * d));
}

float G_Smith(float NdotV, float NdotL, float rough) {
  float r = rough + 1.0;
  float k = (r * r) / 8.0;
  float g_v = NdotV / (NdotV * (1.0 - k) + k);
  float g_l = NdotL / (NdotL * (1.0 - k) + k);
  return g_v * g_l;
}

// screen-space bump from a height field h(uv) in world XZ
vec3 perturb_normal_ws(vec3 N, vec3 world_pos, float h, float scale) {
  vec3 dpdx = dFdx(world_pos);
  vec3 dpdy = dFdy(world_pos);
  vec3 T = normalize(dpdx);
  vec3 B = normalize(cross(N, T));
  float hx = dFdx(h);
  float hy = dFdy(h);
  vec3 Np = normalize(N - scale * (hx * B + hy * T));
  return Np;
}

// hemisphere ambient (sky/ground)
vec3 hemilight(vec3 N) {
  vec3 sky = vec3(0.46, 0.70, 0.82);
  vec3 ground = vec3(0.22, 0.18, 0.14);
  float t = saturate(N.y * 0.5 + 0.5);
  return mix(ground, sky, t) * 0.29;
}

// ---------------------
// main
// ---------------------
void main() {
  vec3 base_color = u_color;
  if (u_useTexture)
    base_color *= texture(u_texture, v_texCoord).rgb;

  vec3 N = normalize(v_normal);
  vec2 uv = v_worldPos.xz * 5.0;

  float avg = (base_color.r + base_color.g + base_color.b) * (1.0 / 3.0);
  float hue_span = max(max(base_color.r, base_color.g), base_color.b) -
                   min(min(base_color.r, base_color.g), base_color.b);

  // Material ID: 0=rider skin, 1=armor, 2=helmet, 3=weapon, 4=shield,
  // 5=rider clothing, 6=horse hide, 7=horse mane, 8=horse hoof,
  // 9=saddle leather, 10=bridle, 11=saddle blanket
  bool is_rider_skin = (u_materialId == 0);
  bool is_body_armor = (u_materialId == 1);
  bool is_helmet = (u_materialId == 2);
  bool is_weapon = (u_materialId == 3);
  bool is_shield = (u_materialId == 4);
  bool is_rider_clothing = (u_materialId == 5);
  bool is_horse_hide = (u_materialId == 6);
  bool is_horse_mane = (u_materialId == 7);
  bool is_horse_hoof = (u_materialId == 8);
  bool is_saddle_leather = (u_materialId == 9);
  bool is_bridle = (u_materialId == 10);
  bool is_saddle_blanket = (u_materialId == 11);

  // Material-based detection only (no fallbacks)
  bool is_brass = is_helmet;
  bool is_steel = false;
  bool is_chain = false;
  bool is_fabric = is_rider_clothing || is_saddle_blanket;
  bool is_leather = is_saddle_leather || is_bridle;

  if (is_rider_skin) {
    // Carthage horse swordsman: light complexion.
    vec3 target = vec3(0.96, 0.86, 0.76);
    float tone_noise = fbm(v_worldPos.xz * 3.1) - 0.5;
    base_color = clamp(target + vec3(tone_noise) * 0.05, 0.0, 1.0);
    if (u_useTexture) {
      vec3 tex = texture(u_texture, v_texCoord).rgb;
      float eye_mask = step(0.25, 1.0 - dot(tex, vec3(0.299, 0.587, 0.114)));
      base_color = mix(base_color, vec3(0.02), eye_mask); // black eyes
    }
  }

  // lighting frame
  vec3 L = normalize(vec3(1.0, 1.2, 1.0));
  vec3 V = normalize(
      vec3(0.0, 1.0, 0.5)); // stable view proxy (keeps interface unchanged)
  vec3 H = normalize(L + V);

  float NdotL = saturate(dot(N, L));
  float NdotV = saturate(dot(N, V));
  float NdotH = saturate(dot(N, H));
  float VdotH = saturate(dot(V, H));

  // wrap diffuse like original (softens lambert)
  float wrap_amount = (avg > 0.50) ? 0.08 : 0.30;
  float NdotL_wrap = max(NdotL * (1.0 - wrap_amount) + wrap_amount, 0.12);

  // base material params
  float roughness = 0.5;
  vec3 F0 = vec3(0.04); // dielectric default
  float metalness = 0.0;
  vec3 albedo = base_color;

  // micro details / masks (re-used)
  float n_small = fbm(uv * 6.0);
  float n_large = fbm(uv * 2.0);
  float cavity = 1.0 - (n_large * 0.25 + n_small * 0.15);

  // ---------------------
  // MATERIAL BRANCHES
  // ---------------------
  vec3 col = vec3(0.0);
  vec3 ambient = hemilight(N) * (0.85 + 0.15 * cavity);

  if (is_body_armor) {
    // Bronze + chain + linen mix to match infantry look.
    float brushed =
        abs(sin(v_worldPos.y * 55.0)) * 0.02 + noise(uv * 28.0) * 0.015;
    float plates = armor_plates(v_worldPos.xz, v_worldPos.y);
    float rings = chainmail_rings(v_worldPos.xz);
    float linen = fbm(uv * 5.0);

    // bump from light hammering
    float h = fbm(vec2(v_worldPos.y * 18.0, v_worldPos.z * 6.0));
    N = perturb_normal_ws(N, v_worldPos, h, 0.32);

    vec3 bronze_tint = vec3(0.62, 0.46, 0.20);
    vec3 steel_tint = vec3(0.68, 0.70, 0.74);
    vec3 linen_tint = vec3(0.86, 0.80, 0.72);
    vec3 leather_tint = vec3(0.38, 0.25, 0.15);

    // Treat entire armor mesh as torso to avoid height-based clipping.
    float torsoBand = 1.0;
    float skirtBand = 0.0;
    float mailBlend =
        clamp(smoothstep(0.15, 0.85, rings + cavity * 0.25), 0.15, 1.0) *
        torsoBand;
    float cuirassBlend = torsoBand;
    float leatherBlend = skirtBand * 0.65;
    float linenBlend = skirtBand * 0.45;

    vec3 bronze = mix(bronze_tint, base_color, 0.40);
    vec3 chain_col = mix(steel_tint, base_color, 0.25);
    vec3 linen_col = mix(linen_tint, base_color, 0.20);
    vec3 leather_col = mix(leather_tint, base_color, 0.30);

    albedo = bronze;
    albedo = mix(albedo, chain_col, mailBlend);
    albedo = mix(albedo, linen_col, linenBlend);
    albedo = mix(albedo, leather_col, leatherBlend);

    // bias toward brighter metal luma
    float armor_luma = dot(albedo, vec3(0.299, 0.587, 0.114));
    albedo = mix(albedo, albedo * 1.20, smoothstep(0.30, 0.65, armor_luma));

    roughness = 0.32 + brushed * 1.2;
    roughness = clamp(roughness, 0.18, 0.55);
    F0 = mix(vec3(0.74), albedo, 0.25);

    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnel_schlick(VdotH, F0);
    vec3 spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    col += ambient * mix(vec3(1.0), albedo, 0.25);
    col += NdotL_wrap * (spec * 1.35);
    col += vec3(plates) * 0.35 + vec3(rings * 0.25) + vec3(linen * linenBlend);

  } else if (is_horse_hide) {
    // Horses: unified natural coat, fully matte, no sparkle.
    vec3 coat = vec3(0.36, 0.32, 0.28);
    float hide_tex = horse_hide_pattern(v_worldPos.xz);
    float grain = fbm(v_worldPos.xz * 18.0 + v_worldPos.y * 2.5);
    albedo = coat * (1.0 + hide_tex * 0.06 - grain * 0.04);

    roughness = 0.75;
    F0 = vec3(0.02);
    metalness = 0.0;

    // Slight normal bump from hair grain without spec pop.
    float h = fbm(v_worldPos.xz * 22.0);
    N = perturb_normal_ws(N, v_worldPos, h, 0.18);

    col += ambient * albedo;
    col += NdotL_wrap * albedo * 0.9;

  } else if (is_horse_mane) {
    // Mane: dark, matte.
    float strand = sin(uv.x * 140.0) * 0.2 + noise(uv * 120.0) * 0.15;
    float clump = noise(uv * 15.0) * 0.4;
    float frizz = fbm(uv * 40.0) * 0.15;

    float h = fbm(v_worldPos.xz * 25.0);
    N = perturb_normal_ws(N, v_worldPos, h, 0.18);

    albedo = vec3(0.08, 0.07, 0.07) *
             (1.0 + strand * 0.02 + clump * 0.02 + frizz * 0.02);

    roughness = 0.70;
    F0 = vec3(0.02);
    metalness = 0.0;

    col += ambient * albedo;
    col += NdotL_wrap * albedo * 0.85;

  } else if (is_steel) {
    float brushed =
        abs(sin(v_worldPos.y * 95.0)) * 0.02 + noise(uv * 35.0) * 0.015;
    float dents = noise(uv * 8.0) * 0.03;
    float plates = armor_plates(v_worldPos.xz, v_worldPos.y);

    // bump from brushing
    float h = fbm(vec2(v_worldPos.y * 25.0, v_worldPos.z * 6.0));
    N = perturb_normal_ws(N, v_worldPos, h, 0.35);

    // steel-like params
    metalness = 1.0;
    F0 = vec3(0.92);
    roughness = 0.28 + brushed * 2.0 + dents * 0.6;
    roughness = clamp(roughness, 0.15, 0.55);

    // base tint & sky reflection lift
    albedo = mix(vec3(0.60), base_color, 0.25);
    float sky_refl = (N.y * 0.5 + 0.5) * 0.10;

    // microfacet spec only for metals
    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnel_schlick(VdotH, F0 * albedo); // slight tint
    vec3 spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    col += ambient * 0.3; // metals rely more on spec
    col += NdotL_wrap * spec * 1.5;
    col += vec3(plates) + vec3(sky_refl) - vec3(dents * 0.25) + vec3(brushed);

  } else if (is_brass) {
    float brass_noise = noise(uv * 22.0) * 0.02;
    float patina = fbm(uv * 4.0) * 0.12; // larger-scale patina

    // bump from subtle hammering
    float h = fbm(uv * 18.0);
    N = perturb_normal_ws(N, v_worldPos, h, 0.30);

    metalness = 1.0;
    vec3 brass_tint = vec3(0.94, 0.78, 0.45);
    F0 = mix(brass_tint, base_color, 0.5);
    roughness = clamp(0.32 + patina * 0.45, 0.18, 0.75);

    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnel_schlick(VdotH, F0);
    vec3 spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    col += ambient * 0.25;
    col += NdotL_wrap * spec * 1.35;
    col += vec3(brass_noise) - vec3(patina * 0.35);

  } else if (is_chain) {
    float rings = chainmail_rings(v_worldPos.xz);
    float ring_hi = noise(uv * 30.0) * 0.10;

    // small pitted bump
    float h = fbm(uv * 35.0);
    N = perturb_normal_ws(N, v_worldPos, h, 0.25);

    metalness = 1.0;
    F0 = vec3(0.86);
    roughness = 0.35;

    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnel_schlick(VdotH, F0);
    vec3 spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    col += ambient * 0.25;
    col += NdotL_wrap * (spec * (1.2 + rings)) + vec3(ring_hi);
    // slight diffuse damping to keep chainmail darker in cavities
    col *= (0.95 - 0.10 * (1.0 - cavity));

  } else if (is_fabric) {
    float weave_x = sin(v_worldPos.x * 70.0);
    float weave_z = sin(v_worldPos.z * 70.0);
    float weave = weave_x * weave_z * 0.04;
    float embroidery = fbm(uv * 6.0) * 0.08;

    float h = fbm(uv * 22.0) * 0.7 + weave * 0.6;
    N = perturb_normal_ws(N, v_worldPos, h, 0.35);

    roughness = 0.78;
    F0 = vec3(0.035);
    metalness = 0.0;

    vec3 F = fresnel_schlick(VdotH, F0);
    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    float sheen = pow(1.0 - NdotV, 6.0) * 0.10;
    albedo *= 1.0 + fbm(uv * 5.0) * 0.10 - 0.05;

    col += ambient * albedo;
    col += NdotL_wrap * (albedo * (1.0 - F) + spec * 0.3) +
           vec3(weave + embroidery + sheen);

  } else { // leather / hooves fallback
    float grain = fbm(uv * 10.0) * 0.15;
    float wear = fbm(uv * 3.0) * 0.12;

    float h = fbm(uv * 18.0);
    N = perturb_normal_ws(N, v_worldPos, h, 0.28);

    roughness = 0.58 - wear * 0.15;
    F0 = vec3(0.038);
    metalness = 0.0;

    vec3 F = fresnel_schlick(VdotH, F0);
    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    float sheen = pow(1.0 - NdotV, 4.0) * 0.06;

    albedo *= (1.0 + grain - 0.06 + wear * 0.05);
    col += ambient * albedo;
    col += NdotL_wrap * (albedo * (1.0 - F)) + spec * 0.4 + vec3(sheen);
  }

  col = mix(col, vec3(0.32, 0.60, 0.66),
            saturate((base_color.g + base_color.b) * 0.4) * 0.12);
  col = saturate(col);
  FragColor = vec4(col, u_alpha);
}
