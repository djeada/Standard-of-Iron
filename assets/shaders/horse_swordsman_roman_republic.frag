#version 330 core

in vec3 v_normal;
in vec3 v_worldNormal;
in vec3 v_tangent;
in vec3 v_bitangent;
in vec2 v_texCoord;
in vec3 v_worldPos;
in float v_armorLayer;
in float v_bodyHeight;
in float v_armorSheen;
in float v_leatherWear;
in float v_horseMusculature;
in float v_hairFlow;
in float v_hoofWear;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_useTexture;
uniform float u_alpha;
uniform int u_materialId;

out vec4 FragColor;

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

float aa_step(float edge, float x) {
  float w = fwidth(x);
  return smoothstep(edge - w, edge + w, x);
}

float armor_plates(vec2 p, float y) {
  float plate_y = fract(y * 6.5);
  float line = smoothstep(0.92, 0.98, plate_y) - smoothstep(0.98, 1.0, plate_y);

  line = smoothstep(0.0, fwidth(plate_y) * 2.0, line) * 0.12;

  float rivet_x = fract(p.x * 18.0);
  float rivet =
      smoothstep(0.48, 0.50, rivet_x) * smoothstep(0.52, 0.50, rivet_x);
  rivet *= step(0.92, plate_y);
  return line + rivet * 0.25;
}

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

vec3 microfacetSpec(float D, float G, vec3 F, float NdotV, float NdotL) {
  if (NdotV <= 0.001 || NdotL <= 0.001) {
    return vec3(0.0);
  }
  float denom = 4.0 * max(NdotV, 0.01) * max(NdotL, 0.01);
  return (D * G) * F / denom;
}

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

vec3 hemilight(vec3 N) {
  vec3 sky = vec3(0.55, 0.64, 0.80);
  vec3 ground = vec3(0.23, 0.20, 0.17);
  float t = saturate(N.y * 0.5 + 0.5);
  return mix(ground, sky, t) * 0.28;
}

void main() {
  vec3 base_color = u_color;
  if (u_useTexture)
    base_color *= texture(u_texture, v_texCoord).rgb;

  vec3 N = normalize(v_normal);
  vec2 uv = v_worldPos.xz * 5.0;

  float avg = (base_color.r + base_color.g + base_color.b) * (1.0 / 3.0);
  float hue_span = max(max(base_color.r, base_color.g), base_color.b) -
                   min(min(base_color.r, base_color.g), base_color.b);

  bool is_rider_skin = (u_materialId == 0);
  bool is_rider_armor = (u_materialId == 1);
  bool is_rider_helmet = (u_materialId == 2);
  bool is_rider_weapon = (u_materialId == 3);
  bool is_rider_shield = (u_materialId == 4);
  bool is_rider_clothing = (u_materialId == 5);

  bool is_horse_hide = (u_materialId == 6);
  bool is_horse_mane = (u_materialId == 7);
  bool is_horse_hoof = (u_materialId == 8);

  bool is_saddle_leather = (u_materialId == 9);
  bool is_bridle = (u_materialId == 10);
  bool is_saddle_blanket = (u_materialId == 11);

  bool is_brass = is_rider_helmet;
  bool is_steel = is_rider_armor;
  bool is_chain = is_rider_armor;
  bool is_fabric = is_rider_clothing || is_saddle_blanket;
  bool is_leather = is_saddle_leather || is_bridle;

  vec3 L = normalize(vec3(1.0, 1.2, 1.0));
  vec3 V = normalize(vec3(0.0, 1.0, 0.5));
  vec3 H = normalize(L + V);

  float NdotL = saturate(dot(N, L));
  float NdotV = saturate(dot(N, V));
  float NdotH = saturate(dot(N, H));
  float VdotH = saturate(dot(V, H));

  float wrap_amount = (avg > 0.50) ? 0.08 : 0.30;
  float NdotL_wrap = max(NdotL * (1.0 - wrap_amount) + wrap_amount, 0.12);

  float roughness = 0.5;
  vec3 F0 = vec3(0.04);
  float metalness = 0.0;
  vec3 albedo = base_color;

  float n_small = fbm(uv * 6.0);
  float n_large = fbm(uv * 2.0);
  float cavity = 1.0 - (n_large * 0.25 + n_small * 0.15);

  vec3 col = vec3(0.0);
  vec3 ambient = hemilight(N) * (0.85 + 0.15 * cavity);

  if (is_horse_hide) {

    float coat_noise = fbm(v_worldPos.xz * 1.2);
    float coat_var = 0.85 + coat_noise * 0.30;

    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 hair_flow = normalize(cross(up, N) + vec3(v_hairFlow * 0.3, 0.0, 0.0));

    float hair_tex = fbm(v_worldPos.xz * 120.0) * 0.12;

    float sheen_falloff = pow(saturate(dot(V, reflect(L, N))), 3.5);
    float coat_sheen = sheen_falloff * (0.15 + 0.10 * coat_var);

    float muscle_shadow = v_horseMusculature * 0.18;
    float shadow_pattern = smoothstep(0.40, 0.70, v_horseMusculature);
    muscle_shadow *= shadow_pattern;

    float dapple_noise =
        fbm(v_worldPos.xz * 4.5 + vec2(v_bodyHeight * 2.0, 0.0));
    float dapples = smoothstep(0.75, 0.85, dapple_noise) * 0.14;

    dapples *= smoothstep(0.45, 0.60, v_bodyHeight);

    float sweat_zone = smoothstep(0.30, 0.20, v_bodyHeight);
    sweat_zone += smoothstep(0.55, 0.65, v_bodyHeight) * 0.4;
    float moisture = sweat_zone * (0.3 + 0.2 * fbm(v_worldPos.xz * 8.0));
    moisture = saturate(moisture);

    float dirt_height = smoothstep(0.35, 0.15, v_bodyHeight);
    float dirt_pattern =
        fbm(v_worldPos.xz * 15.0 + vec2(0.0, v_bodyHeight * 5.0));
    float dirt = dirt_height * (0.5 + 0.5 * dirt_pattern) * 0.25;

    vec3 T = normalize(hair_flow);
    float flow_noise = fbm(v_worldPos.xz * 10.0);
    float aniso_highlight =
        pow(saturate(dot(H, T)), 18.0) * (0.6 + 0.4 * flow_noise);
    aniso_highlight *= 0.08 * (1.0 - moisture * 0.5);

    albedo = albedo * coat_var;
    albedo = albedo * (1.0 - muscle_shadow);
    albedo = albedo * (1.0 + dapples);
    albedo = albedo * (1.0 - moisture * 0.35);
    albedo = mix(albedo, albedo * 0.60, dirt);
    albedo = albedo * (1.0 + hair_tex * 0.15);

    roughness = 0.58 - coat_sheen * 0.12 + moisture * 0.15 + dirt * 0.20;
    roughness = saturate(roughness);
    F0 = vec3(0.035);
    metalness = 0.0;

    float hair_bump = fbm(v_worldPos.xz * 35.0) + hair_tex * 0.5;
    N = perturb_normal_ws(N, v_worldPos, hair_bump, 0.45);

    col += ambient * albedo;

    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnel_schlick(VdotH, F0);
    vec3 spec = microfacetSpec(D, G, F, NdotV, NdotL);

    col += NdotL_wrap * (albedo * (1.0 - F) * 0.95) + spec * 0.8;

    col += aniso_highlight * vec3(0.9, 0.85, 0.80);
    col += coat_sheen * vec3(1.0, 0.98, 0.95) * NdotL;

  } else if (is_horse_mane) {

    float hair_strand_noise =
        fbm(v_worldPos.xz * 80.0 + vec2(0.0, v_worldPos.y * 15.0));
    float hair_strands = hair_strand_noise * 0.35;

    float clump_noise = fbm(v_worldPos.xz * 20.0);
    float clumping = smoothstep(0.35, 0.65, clump_noise) * 0.25;

    vec3 gravity_dir = vec3(0.0, -1.0, 0.0);
    vec3 hair_flow_dir = normalize(mix(gravity_dir, N, v_hairFlow * 0.4));

    vec3 T = normalize(cross(hair_flow_dir, vec3(1.0, 0.0, 0.0)) + 1e-4);
    float TdotL = dot(T, L);
    float TdotV = dot(T, V);

    float primary_shift = 0.05;
    float primary_spec =
        sqrt(1.0 - TdotL * TdotL) * sqrt(1.0 - TdotV * TdotV) - TdotL * TdotV;
    primary_spec = saturate(primary_spec + primary_shift);
    primary_spec = pow(primary_spec, 18.0);

    float secondary_shift = 0.15;
    float secondary_spec =
        sqrt(1.0 - TdotL * TdotL) * sqrt(1.0 - TdotV * TdotV) - TdotL * TdotV;
    secondary_spec = saturate(secondary_spec + secondary_shift);
    secondary_spec = pow(secondary_spec, 8.0);

    float coarse_tex =
        fbm(v_worldPos.xz * 150.0 + vec2(v_worldPos.y * 25.0, 0.0));
    coarse_tex = coarse_tex * 0.20;

    albedo = albedo * 0.75;
    albedo = albedo * (1.0 - clumping * 0.3);
    albedo = albedo * (1.0 + hair_strands * 0.15);
    albedo = albedo * (1.0 + coarse_tex * 0.10);

    roughness = 0.65 + coarse_tex * 0.15;
    roughness = saturate(roughness);
    F0 = vec3(0.045);
    metalness = 0.0;

    float hair_bump = hair_strand_noise * 2.0 + clump_noise * 0.8;
    N = perturb_normal_ws(N, v_worldPos, hair_bump, 0.65);

    col += ambient * albedo * 0.90;

    col += NdotL_wrap * albedo * 0.75;

    col += primary_spec * vec3(0.85, 0.80, 0.75) * 0.35;
    col += secondary_spec * vec3(0.95, 0.92, 0.88) * 0.18;

    float rim = pow(1.0 - NdotV, 3.0) * 0.12;
    col += rim * vec3(0.9, 0.85, 0.80);

  } else if (is_horse_hoof) {

    albedo = albedo * vec3(0.15, 0.12, 0.10);

    float ring_height = v_worldPos.y * 45.0;
    float growth_rings = abs(sin(ring_height)) * 0.08;

    growth_rings *= smoothstep(0.0, 0.15, v_bodyHeight);

    float chip_noise = fbm(v_worldPos.xz * 55.0);
    float chipping = smoothstep(0.65, 0.85, chip_noise) * v_hoofWear;

    chipping *= smoothstep(0.08, 0.02, v_bodyHeight);

    float ground_contact = smoothstep(0.10, 0.03, v_bodyHeight);
    float keratin_sheen =
        pow(saturate(dot(V, reflect(L, N))), 15.0) * ground_contact;
    keratin_sheen *= 0.18;

    float crevice_noise =
        fbm(v_worldPos.xz * 30.0 + vec2(0.0, v_worldPos.y * 20.0));
    float crevices = smoothstep(0.45, 0.55, crevice_noise) * 0.12;

    float dirt_accum = smoothstep(0.15, 0.05, v_bodyHeight) * 0.35;
    float dirt_pattern = fbm(v_worldPos.xz * 18.0);
    float dirt = dirt_accum * (0.6 + 0.4 * dirt_pattern);

    float keratin_tex =
        fbm(v_worldPos.xz * 80.0 + vec2(v_worldPos.y * 35.0, 0.0));
    keratin_tex *= 0.08;

    albedo = albedo * (1.0 + growth_rings);
    albedo = albedo * (1.0 - chipping * 0.4);
    albedo = mix(albedo, albedo * 1.8, dirt * 0.7);
    albedo = albedo * (1.0 + keratin_tex * 0.12);
    albedo = albedo * (1.0 + crevices * 0.15);

    roughness = 0.45 - ground_contact * 0.20 + dirt * 0.30 + chipping * 0.25;
    roughness = saturate(roughness);
    F0 = vec3(0.042);
    metalness = 0.0;

    float hoof_bump =
        keratin_tex * 2.0 + growth_rings * 1.5 + chip_noise * chipping * 3.0;
    N = perturb_normal_ws(N, v_worldPos, hoof_bump, 0.55);

    col += ambient * albedo;

    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnel_schlick(VdotH, F0);
    vec3 spec = microfacetSpec(D, G, F, NdotV, NdotL);

    col += NdotL_wrap * (albedo * (1.0 - F)) + spec * 0.75;

    col += keratin_sheen * vec3(0.95, 0.93, 0.90);

  } else if (is_saddle_leather) {

    float leather_age = v_leatherWear * 0.4;
    albedo = albedo * vec3(0.85, 0.68, 0.52) * (1.0 - leather_age);

    float grain_noise =
        fbm(v_worldPos.xz * 40.0 + vec2(v_worldPos.y * 12.0, 0.0));
    float grain = grain_noise * 0.18 * (1.0 + v_leatherWear * 0.5);

    float tool_marks = abs(sin(v_worldPos.x * 28.0 + grain_noise * 3.0)) * 0.06;
    tool_marks *= smoothstep(0.3, 0.7, fbm(v_worldPos.xz * 8.0));

    float contact_wear = smoothstep(0.4, 0.6, v_bodyHeight) * v_leatherWear;
    contact_wear *= (0.7 + 0.3 * fbm(v_worldPos.xz * 15.0));

    float crease_pattern = abs(sin(v_worldPos.y * 35.0 + v_worldPos.x * 8.0));
    float creases =
        smoothstep(0.85, 0.95, crease_pattern) * v_leatherWear * 0.15;

    float oil_accum = fbm(v_worldPos.xz * 25.0) * 0.20;
    oil_accum *= (0.6 + v_leatherWear * 0.4);

    float edge_noise = fbm(v_worldPos.xz * 50.0);
    float edges = smoothstep(0.75, 0.90, edge_noise) * 0.12;

    float stitch_line =
        abs(sin(v_worldPos.x * 45.0)) * abs(sin(v_worldPos.z * 45.0));
    float stitching = smoothstep(0.92, 0.98, stitch_line) * 0.08;

    albedo = albedo * (1.0 + grain);
    albedo = albedo * (1.0 - tool_marks * 0.5);
    albedo = albedo * (1.0 - contact_wear * 0.35);
    albedo = albedo * (1.0 - creases * 0.4);
    albedo = mix(albedo, albedo * 0.70, oil_accum);
    albedo = albedo * (1.0 + edges * 0.3);
    albedo = albedo * (1.0 + stitching * 0.15);

    roughness = 0.55 + v_leatherWear * 0.25 - oil_accum * 0.20;
    roughness = saturate(roughness);
    F0 = vec3(0.038);
    metalness = 0.0;

    float leather_bump = grain_noise * 2.0 + creases * 2.5 + tool_marks * 1.5;
    N = perturb_normal_ws(N, v_worldPos, leather_bump, 0.50);

    col += ambient * albedo;

    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnel_schlick(VdotH, F0);
    vec3 spec = microfacetSpec(D, G, F, NdotV, NdotL);

    col += NdotL_wrap * (albedo * (1.0 - F)) + spec * 0.65;

    float leather_sheen = pow(1.0 - NdotV, 4.0) * contact_wear * 0.08;
    col += leather_sheen * vec3(0.90, 0.85, 0.75);

  } else if (is_bridle) {

    albedo = albedo * vec3(0.45, 0.35, 0.28) * (1.0 - v_leatherWear * 0.3);

    float strap_grain =
        fbm(v_worldPos.xz * 55.0 + vec2(v_worldPos.y * 20.0, 0.0));
    strap_grain *= 0.15;

    float stitch_freq = 42.0;
    float stitch_x = abs(sin(v_worldPos.x * stitch_freq));
    float stitch_z = abs(sin(v_worldPos.z * stitch_freq));
    float stitches = max(smoothstep(0.90, 0.96, stitch_x),
                         smoothstep(0.90, 0.96, stitch_z)) *
                     0.12;

    float contact_noise = fbm(v_worldPos.xz * 22.0);
    float strap_wear = contact_noise * v_leatherWear * 0.40;

    float metal_pattern = step(0.85, fbm(v_worldPos.xz * 18.0)) * 0.25;
    vec3 bronze_color = vec3(0.72, 0.55, 0.35);
    vec3 patina_color = vec3(0.35, 0.52, 0.42);
    float patina = fbm(v_worldPos.xz * 60.0) * 0.35;
    vec3 metal_color = mix(bronze_color, patina_color, patina);

    float flex_crease = abs(sin(v_worldPos.y * 40.0 + v_worldPos.x * 15.0));
    float creasing = smoothstep(0.80, 0.92, flex_crease) * v_leatherWear * 0.18;

    float sweat_stain = smoothstep(0.45, 0.65, v_bodyHeight) * 0.25;
    sweat_stain *= fbm(v_worldPos.xz * 12.0);

    albedo = albedo * (1.0 + strap_grain);
    albedo = albedo * (1.0 + stitches * 0.5);
    albedo = mix(albedo, albedo * 0.65, strap_wear);
    albedo = mix(albedo, metal_color, metal_pattern);
    albedo = albedo * (1.0 - creasing * 0.35);
    albedo = mix(albedo, albedo * 0.75, sweat_stain);

    float is_metal_area = metal_pattern;
    roughness =
        mix(0.60 + v_leatherWear * 0.20, 0.35 + patina * 0.25, is_metal_area);
    roughness = saturate(roughness);
    F0 = mix(vec3(0.040), vec3(0.65), is_metal_area);
    metalness = is_metal_area * 0.8;

    float bridle_bump = strap_grain * 2.0 + stitches * 3.0 + creasing * 2.0;
    N = perturb_normal_ws(N, v_worldPos, bridle_bump, 0.55);

    col += ambient * albedo;

    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnel_schlick(VdotH, F0);
    vec3 spec = microfacetSpec(D, G, F, NdotV, NdotL);

    col += NdotL_wrap * (albedo * (1.0 - F)) + spec * 0.70;

    col += is_metal_area * pow(saturate(dot(N, H)), 25.0) * 0.15 *
           vec3(0.95, 0.88, 0.75);

  } else if (is_saddle_blanket) {

    vec3 fabric_base = base_color;

    float warp_threads = abs(sin(v_worldPos.x * 65.0));
    float weft_threads = abs(sin(v_worldPos.z * 65.0));
    float weave_pattern = (warp_threads * weft_threads) * 0.18;

    float thread_noise = fbm(v_worldPos.xz * 80.0);
    float thread_var = thread_noise * 0.12;

    float dye_variation = fbm(v_worldPos.xz * 3.5) * 0.15;

    float pilling_noise = fbm(v_worldPos.xz * 45.0);
    float pilling =
        smoothstep(0.60, 0.80, pilling_noise) * v_leatherWear * 0.20;

    float center_stain = smoothstep(0.45, 0.65, v_bodyHeight) * 0.40;
    float stain_pattern = fbm(v_worldPos.xz * 8.0);
    float sweat_stain = center_stain * (0.7 + 0.3 * stain_pattern);

    float edge_noise = fbm(v_worldPos.xz * 55.0);
    float fraying = smoothstep(0.80, 0.95, edge_noise) * v_leatherWear * 0.15;

    float compression_pattern = smoothstep(0.50, 0.65, v_bodyHeight);
    float compression = compression_pattern * fbm(v_worldPos.xz * 20.0) * 0.12;

    float dirt_pattern = fbm(v_worldPos.xz * 18.0);
    float dirt = v_leatherWear * dirt_pattern * 0.30;
    vec3 dirt_color = vec3(0.32, 0.28, 0.24);

    albedo = fabric_base;
    albedo = albedo * (1.0 + dye_variation);
    albedo = albedo * (1.0 + weave_pattern);
    albedo = albedo * (1.0 + thread_var);
    albedo = albedo * (1.0 + pilling * 0.5);
    albedo = mix(albedo, albedo * 0.60, sweat_stain);
    albedo = albedo * (1.0 - fraying * 0.4);
    albedo = albedo * (1.0 - compression * 0.3);
    albedo = mix(albedo, dirt_color, dirt);

    roughness = 0.75 + pilling * 0.15 + sweat_stain * 0.10;
    roughness = saturate(roughness);
    F0 = vec3(0.028);
    metalness = 0.0;

    float fabric_bump =
        weave_pattern * 2.5 + pilling * 2.0 + thread_noise * 1.5;
    N = perturb_normal_ws(N, v_worldPos, fabric_bump, 0.60);

    col += ambient * albedo * 1.05;

    col += NdotL_wrap * albedo * 0.95;

    float fabric_sheen = sweat_stain * compression * 0.05;
    col += fabric_sheen * vec3(0.85, 0.82, 0.78);

  } else if (is_rider_armor) {

    bool is_muscle_cuirass =
        (v_bodyHeight > 0.55 && v_bodyHeight < 0.75 && v_armorSheen > 0.75);

    if (is_muscle_cuirass) {

      vec3 bronze_base = vec3(0.72, 0.58, 0.42) * base_color;

      float muscle_y = v_worldPos.y * 12.0;
      float muscle_pattern = abs(sin(muscle_y)) * abs(cos(muscle_y * 0.5));
      float muscle_def = smoothstep(0.40, 0.70, muscle_pattern) * 0.22;

      float ab_bands = abs(sin(v_worldPos.y * 18.0)) * 0.14;
      ab_bands *= smoothstep(0.58, 0.68, v_bodyHeight);

      float edge_noise = fbm(v_worldPos.xz * 35.0);
      float edges = smoothstep(0.80, 0.92, edge_noise) * 0.16;

      float decor_pattern = step(0.85, fbm(v_worldPos.xz * 8.0)) * 0.20;
      decor_pattern *= smoothstep(0.60, 0.65, v_bodyHeight);

      float patina_pattern = fbm(v_worldPos.xz * 5.5);
      float patina = patina_pattern * 0.22;
      vec3 patina_color = vec3(0.35, 0.52, 0.42);

      float dent_noise = fbm(v_worldPos.xz * 12.0);
      float dents = smoothstep(0.78, 0.88, dent_noise) * 0.12;

      float strap_pattern =
          abs(sin(v_worldPos.x * 8.0)) * abs(sin(v_worldPos.z * 45.0));
      float straps = smoothstep(0.92, 0.98, strap_pattern) * 0.12;
      vec3 leather_color = vec3(0.35, 0.28, 0.22);

      albedo = bronze_base;
      albedo = albedo * (1.0 + muscle_def);
      albedo = albedo * (1.0 + ab_bands);
      albedo = albedo * (1.0 + edges * 0.5);
      albedo = albedo * (1.0 + decor_pattern * 0.7);
      albedo = mix(albedo, patina_color, patina);
      albedo = albedo * (1.0 - dents * 0.4);
      albedo = mix(albedo, leather_color, straps * 0.3);

      roughness = 0.18 + patina * 0.30 + dents * 0.12;
      roughness = saturate(roughness);
      F0 = vec3(0.70);
      metalness = 0.96;

      float cuirass_bump =
          muscle_def * 3.2 + ab_bands * 2.8 + decor_pattern * 2.2;
      N = perturb_normal_ws(N, v_worldPos, cuirass_bump, 0.62);

    } else {

      float scale_x = v_worldPos.x * 30.0;
      float scale_y = v_worldPos.y * 30.0;

      float row_offset = mod(floor(scale_y), 2.0) * 0.5;
      scale_x += row_offset;

      vec2 scale_uv = fract(vec2(scale_x, scale_y)) - 0.5;
      float scale_dist = max(abs(scale_uv.x) * 1.2, abs(scale_uv.y)) - 0.35;
      float scale = smoothstep(0.05, -0.05, scale_dist);

      float overlap_pattern = smoothstep(0.4, 0.5, fract(scale_y));
      float overlap = scale * overlap_pattern * 0.25;

      float scale_edge = smoothstep(-0.02, 0.02, abs(scale_dist)) * scale;
      float edge_lighting = scale_edge * pow(saturate(dot(N, L)), 2.0) * 0.35;

      float scale_variation = hash(floor(vec2(scale_x, scale_y)));
      float scale_patina = scale_variation * 0.25;

      bool is_iron_scale = (scale_variation > 0.65);
      vec3 bronze_color = vec3(0.72, 0.58, 0.42);
      vec3 iron_color = vec3(0.60, 0.60, 0.62);
      vec3 metal_color =
          mix(bronze_color, iron_color, step(0.65, scale_variation));

      vec3 patina_color = vec3(0.35, 0.52, 0.42);
      metal_color = mix(metal_color, patina_color,
                        scale_patina * (1.0 - float(is_iron_scale)));

      float fabric_gap = (1.0 - scale) * 0.40;
      vec3 fabric_color = vec3(0.42, 0.38, 0.35);

      float damage_pattern = fbm(v_worldPos.xz * 8.0);
      float damage = smoothstep(0.82, 0.92, damage_pattern) * 0.20;

      albedo = metal_color * scale;
      albedo = mix(albedo, fabric_color, fabric_gap);
      albedo = albedo * (1.0 + overlap);
      albedo = albedo * (1.0 - damage * 0.5);
      albedo = albedo * base_color * 0.7;

      float metal_area = scale * (1.0 - damage);
      roughness = mix(0.75, 0.32 + scale_patina * 0.30, metal_area);
      roughness = saturate(roughness);
      F0 = mix(vec3(0.030), vec3(0.65), metal_area);
      metalness = metal_area * 0.85;

      float scale_height = scale * 2.0 + overlap * 1.5;
      N = perturb_normal_ws(N, v_worldPos, scale_height, 0.70);

      albedo = albedo * (1.0 + edge_lighting);
    }

    col += ambient * albedo * 0.85;

    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnel_schlick(VdotH, F0);
    vec3 spec = microfacetSpec(D, G, F, NdotV, NdotL);

    col += NdotL_wrap * (albedo * (1.0 - F) * 0.60 + spec * 1.30);

    float armor_rim = pow(1.0 - NdotV, 4.0) * 0.16;
    col += armor_rim * vec3(0.95, 0.90, 0.82);

  } else if (is_rider_helmet) {

    bool is_crest_holder = (v_bodyHeight > 0.85);
    bool is_cheek_guard = (v_bodyHeight < 0.30);
    bool is_bowl = (!is_crest_holder && !is_cheek_guard);

    vec3 bronze_base = vec3(0.72, 0.58, 0.42) * base_color;

    if (is_bowl) {

      float hammer_marks = fbm(v_worldPos.xz * 25.0) * 0.12;

      float ribbing_pattern =
          abs(sin(v_worldPos.y * 20.0 + v_worldPos.x * 5.0));
      float ribbing = smoothstep(0.88, 0.94, ribbing_pattern) * 0.15;

      float patina_pattern = fbm(v_worldPos.xz * 6.0);
      float patina = patina_pattern * 0.25;
      vec3 patina_color = vec3(0.35, 0.52, 0.42);

      float dent_pattern = fbm(v_worldPos.xz * 10.0);
      float dents = smoothstep(0.80, 0.90, dent_pattern) * 0.15;

      float polish_pattern = smoothstep(0.60, 0.75, v_bodyHeight);
      float polish = polish_pattern * (1.0 - patina * 0.5);

      albedo = bronze_base;
      albedo = albedo * (1.0 + hammer_marks);
      albedo = albedo * (1.0 + ribbing * 0.6);
      albedo = mix(albedo, patina_color, patina);
      albedo = albedo * (1.0 - dents * 0.35);

      roughness = 0.22 - polish * 0.12 + patina * 0.32 + dents * 0.18;

    } else if (is_cheek_guard) {

      float guard_texture = fbm(v_worldPos.xz * 35.0) * 0.10;

      float rivet_pattern = step(0.90, fbm(v_worldPos.xz * 22.0)) * 0.15;

      float guard_patina = fbm(v_worldPos.xz * 7.0) * 0.32;
      vec3 patina_color = vec3(0.35, 0.52, 0.42);

      float scratches = abs(sin(v_worldPos.x * 60.0)) * 0.08;

      albedo = bronze_base;
      albedo = albedo * (1.0 + guard_texture);
      albedo = albedo * (1.0 + rivet_pattern * 0.4);
      albedo = mix(albedo, patina_color, guard_patina);
      albedo = albedo * (1.0 - scratches * 0.5);

      roughness = 0.30 + guard_patina * 0.35;

    } else {

      float ridge_pattern = abs(sin(v_worldPos.x * 15.0)) * 0.18;

      float engraving = fbm(v_worldPos.xz * 12.0) * 0.16;

      float top_patina = fbm(v_worldPos.xz * 5.0) * 0.18;
      vec3 patina_color = vec3(0.35, 0.52, 0.42);

      albedo = bronze_base;
      albedo = albedo * (1.0 + ridge_pattern);
      albedo = albedo * (1.0 + engraving * 0.6);
      albedo = mix(albedo, patina_color, top_patina);

      roughness = 0.25 + top_patina * 0.28;
    }

    roughness = saturate(roughness);
    F0 = vec3(0.68);
    metalness = 0.94;

    float helmet_bump = fbm(v_worldPos.xz * 28.0) * 2.0;
    N = perturb_normal_ws(N, v_worldPos, helmet_bump, 0.55);

    col += ambient * albedo * 0.82;

    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnel_schlick(VdotH, F0);
    vec3 spec = microfacetSpec(D, G, F, NdotV, NdotL);

    col += NdotL_wrap * (albedo * (1.0 - F) * 0.55 + spec * 1.40);

    float helmet_rim = pow(1.0 - NdotV, 3.5) * 0.20;
    col += helmet_rim * vec3(0.95, 0.88, 0.75);

  } else if (is_steel) {
    float brushed =
        abs(sin(v_worldPos.y * 95.0)) * 0.02 + noise(uv * 35.0) * 0.015;
    float dents = noise(uv * 8.0) * 0.03;
    float plates = armor_plates(v_worldPos.xz, v_worldPos.y);

    float h = fbm(vec2(v_worldPos.y * 25.0, v_worldPos.z * 6.0));
    N = perturb_normal_ws(N, v_worldPos, h, 0.35);

    metalness = 1.0;
    F0 = vec3(0.92);
    roughness = 0.28 + brushed * 2.0 + dents * 0.6;
    roughness = clamp(roughness, 0.15, 0.55);

    albedo = mix(vec3(0.60), base_color, 0.25);
    float sky_refl = (N.y * 0.5 + 0.5) * 0.10;

    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnel_schlick(VdotH, F0 * albedo);
    vec3 spec = microfacetSpec(D, G, F, NdotV, NdotL);

    col += ambient * 0.3;
    col += NdotL_wrap * spec * 1.5;
    col += vec3(plates) + vec3(sky_refl) - vec3(dents * 0.25) + vec3(brushed);

  } else if (is_brass) {
    float brass_noise = noise(uv * 22.0) * 0.02;
    float patina = fbm(uv * 4.0) * 0.12;

    float h = fbm(uv * 18.0);
    N = perturb_normal_ws(N, v_worldPos, h, 0.30);

    metalness = 1.0;
    vec3 brass_tint = vec3(0.94, 0.78, 0.45);
    F0 = mix(brass_tint, base_color, 0.5);
    roughness = clamp(0.32 + patina * 0.45, 0.18, 0.75);

    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnel_schlick(VdotH, F0);
    vec3 spec = microfacetSpec(D, G, F, NdotV, NdotL);

    col += ambient * 0.25;
    col += NdotL_wrap * spec * 1.35;
    col += vec3(brass_noise) - vec3(patina * 0.35);

  } else if (is_chain) {
    float rings = chainmail_rings(v_worldPos.xz);
    float ring_hi = noise(uv * 30.0) * 0.10;

    float h = fbm(uv * 35.0);
    N = perturb_normal_ws(N, v_worldPos, h, 0.25);

    metalness = 1.0;
    F0 = vec3(0.86);
    roughness = 0.35;

    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnel_schlick(VdotH, F0);
    vec3 spec = microfacetSpec(D, G, F, NdotV, NdotL);

    col += ambient * 0.25;
    col += NdotL_wrap * (spec * (1.2 + rings)) + vec3(ring_hi);

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
    vec3 spec = microfacetSpec(D, G, F, NdotV, NdotL);

    float sheen = pow(1.0 - NdotV, 6.0) * 0.10;
    albedo *= 1.0 + fbm(uv * 5.0) * 0.10 - 0.05;

    col += ambient * albedo;
    col += NdotL_wrap * (albedo * (1.0 - F) + spec * 0.3) +
           vec3(weave + embroidery + sheen);

  } else {
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
    vec3 spec = microfacetSpec(D, G, F, NdotV, NdotL);

    float sheen = pow(1.0 - NdotV, 4.0) * 0.06;

    albedo *= (1.0 + grain - 0.06 + wear * 0.05);
    col += ambient * albedo;
    col += NdotL_wrap * (albedo * (1.0 - F)) + spec * 0.4 + vec3(sheen);
  }

  col = saturate(col);
  FragColor = vec4(col, u_alpha);
}
