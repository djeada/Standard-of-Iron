#version 330 core

// ============================================================================
// INPUTS & OUTPUTS
// ============================================================================

in vec3 v_normal;
in vec3 v_worldNormal;
in vec3 v_tangent;
in vec3 v_bitangent;
in vec2 v_texCoord;
in vec3 v_worldPos;
in float v_armorLayer; // Armor layer from vertex shader
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

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================
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

// ============================================================================
// MATERIAL PATTERN FUNCTIONS (Horse & Rider)
// ============================================================================

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

// ============================================================================
// PBR FUNCTIONS (Microfacet BRDF)
// ============================================================================

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
  vec3 sky = vec3(0.55, 0.64, 0.80);
  vec3 ground = vec3(0.23, 0.20, 0.17);
  float t = saturate(N.y * 0.5 + 0.5);
  return mix(ground, sky, t) * 0.28;
}

// ============================================================================
// MAIN FRAGMENT SHADER
// ============================================================================

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

  // === MATERIAL ID ROUTING ===
  // Rider materials
  bool is_rider_skin = (u_materialId == 0);
  bool is_rider_armor = (u_materialId == 1);
  bool is_rider_helmet = (u_materialId == 2);
  bool is_rider_weapon = (u_materialId == 3);
  bool is_rider_shield = (u_materialId == 4);
  bool is_rider_clothing = (u_materialId == 5);

  // Horse materials
  bool is_horse_hide = (u_materialId == 6);
  bool is_horse_mane = (u_materialId == 7);
  bool is_horse_hoof = (u_materialId == 8);

  // Tack materials
  bool is_saddle_leather = (u_materialId == 9);
  bool is_bridle = (u_materialId == 10);
  bool is_saddle_blanket = (u_materialId == 11);

  // Material-based detection only (no fallbacks)
  bool is_brass = is_rider_helmet;
  bool is_steel = is_rider_armor;
  bool is_chain = is_rider_armor;
  bool is_fabric = is_rider_clothing || is_saddle_blanket;
  bool is_leather = is_saddle_leather || is_bridle;

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

  if (is_horse_hide) {
    // =====================================================================
    // ENHANCED HORSE HIDE RENDERING (Phase 2)
    // Multi-layer procedural horse coat with anatomical accuracy
    // =====================================================================

    // Base coat color variation (bay horse: reddish-brown body)
    // Using worldspace position for consistent coat pattern across animation
    float coat_noise = fbm(v_worldPos.xz * 1.2);
    float coat_var =
        0.85 + coat_noise * 0.30; // 0.85-1.15 range for natural variation

    // Hair direction flow follows musculature (v_horseMusculature computed in
    // vertex) Hair grows from spine outward/downward
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 hair_flow = normalize(cross(up, N) + vec3(v_hairFlow * 0.3, 0.0, 0.0));

    // Individual hair texture (fine detail at 120x frequency)
    float hair_tex = fbm(v_worldPos.xz * 120.0) * 0.12;

    // Coat sheen (healthy horse = glossy, following hair direction)
    float sheen_falloff = pow(saturate(dot(V, reflect(L, N))), 3.5);
    float coat_sheen = sheen_falloff * (0.15 + 0.10 * coat_var);

    // Muscle definition shadows (visible on flanks, shoulders)
    // v_horseMusculature peaks at mid-torso, fades at extremities
    float muscle_shadow = v_horseMusculature * 0.18;
    float shadow_pattern = smoothstep(0.40, 0.70, v_horseMusculature);
    muscle_shadow *= shadow_pattern;

    // Dappling pattern (some horses have spotted coat on rump/shoulders)
    // Frequency 4.5x for hand-sized spots, smoothstep for soft edges
    float dapple_noise =
        fbm(v_worldPos.xz * 4.5 + vec2(v_bodyHeight * 2.0, 0.0));
    float dapples = smoothstep(0.75, 0.85, dapple_noise) * 0.14;
    // Dapples only on upper body (body_height > 0.5)
    dapples *= smoothstep(0.45, 0.60, v_bodyHeight);

    // Sweat/moisture zones (lower body, under tack area)
    // Darkens coat, increases roughness
    float sweat_zone = smoothstep(0.30, 0.20, v_bodyHeight);  // legs darker
    sweat_zone += smoothstep(0.55, 0.65, v_bodyHeight) * 0.4; // saddle area
    float moisture = sweat_zone * (0.3 + 0.2 * fbm(v_worldPos.xz * 8.0));
    moisture = saturate(moisture);

    // Dirt accumulation (legs, belly, procedural based on height)
    float dirt_height =
        smoothstep(0.35, 0.15, v_bodyHeight); // heavy on lower legs
    float dirt_pattern =
        fbm(v_worldPos.xz * 15.0 + vec2(0.0, v_bodyHeight * 5.0));
    float dirt = dirt_height * (0.5 + 0.5 * dirt_pattern) * 0.25;

    // Anisotropic highlight along hair flow (subtle, horse hair is coarser than
    // human)
    vec3 T = normalize(hair_flow);
    float flow_noise = fbm(v_worldPos.xz * 10.0);
    float aniso_highlight =
        pow(saturate(dot(H, T)), 18.0) * (0.6 + 0.4 * flow_noise);
    aniso_highlight *= 0.08 * (1.0 - moisture * 0.5); // reduced when wet

    // Composite coat color
    // Base albedo darkened by: muscle shadows, moisture, dirt
    // Lightened by: dapples, coat variation
    albedo = albedo * coat_var;
    albedo = albedo * (1.0 - muscle_shadow);
    albedo = albedo * (1.0 + dapples);
    albedo = albedo * (1.0 - moisture * 0.35); // wet coat darker
    albedo = mix(albedo, albedo * 0.60, dirt); // dirt desaturates and darkens
    albedo = albedo * (1.0 + hair_tex * 0.15); // fine hair detail

    // Material properties
    // Roughness: base 0.58, smoother with sheen, rougher when wet/dirty
    roughness = 0.58 - coat_sheen * 0.12 + moisture * 0.15 + dirt * 0.20;
    roughness = saturate(roughness);
    F0 = vec3(0.035); // low specular for organic hair
    metalness = 0.0;

    // Normal perturbation from hair grain (stronger than simple version)
    float hair_bump = fbm(v_worldPos.xz * 35.0) + hair_tex * 0.5;
    N = perturb_normal_ws(N, v_worldPos, hair_bump, 0.45);

    // Lighting composition
    col += ambient * albedo;

    // Microfacet specular (PBR)
    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnel_schlick(VdotH, F0);
    vec3 spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    col += NdotL_wrap * (albedo * (1.0 - F) * 0.95) + spec * 0.8;

    // Add anisotropic hair highlight and coat sheen
    col += aniso_highlight * vec3(0.9, 0.85, 0.80);    // warm highlight
    col += coat_sheen * vec3(1.0, 0.98, 0.95) * NdotL; // bright sheen when lit

  } else if (is_horse_mane) {
    // =====================================================================
    // HORSE MANE & TAIL RENDERING (Phase 2)
    // Coarse hair with anisotropic Kajiya-Kay lighting model
    // =====================================================================

    // Mane/tail hair is coarser than body coat (80x frequency vs 120x)
    float hair_strand_noise =
        fbm(v_worldPos.xz * 80.0 + vec2(0.0, v_worldPos.y * 15.0));
    float hair_strands = hair_strand_noise * 0.35;

    // Hair clumping (manes separate into sections naturally)
    // Clumps are 3-5cm wide, giving ~20x frequency for clump pattern
    float clump_noise = fbm(v_worldPos.xz * 20.0);
    float clumping = smoothstep(0.35, 0.65, clump_noise) * 0.25;

    // Flow direction (falls with gravity + movement)
    // v_hairFlow = 0.5-1.0 computed in vertex shader
    vec3 gravity_dir = vec3(0.0, -1.0, 0.0);
    vec3 hair_flow_dir = normalize(mix(gravity_dir, N, v_hairFlow * 0.4));

    // Anisotropic Kajiya-Kay highlight (hair-specific lighting)
    // Tangent along hair flow direction
    vec3 T = normalize(cross(hair_flow_dir, vec3(1.0, 0.0, 0.0)) + 1e-4);
    float TdotL = dot(T, L);
    float TdotV = dot(T, V);

    // Primary highlight (specular along hair strand)
    float primary_shift = 0.05; // slight offset for realism
    float primary_spec =
        sqrt(1.0 - TdotL * TdotL) * sqrt(1.0 - TdotV * TdotV) - TdotL * TdotV;
    primary_spec = saturate(primary_spec + primary_shift);
    primary_spec = pow(primary_spec, 18.0); // tight highlight for coarse hair

    // Secondary highlight (softer, shifted)
    float secondary_shift = 0.15;
    float secondary_spec =
        sqrt(1.0 - TdotL * TdotL) * sqrt(1.0 - TdotV * TdotV) - TdotL * TdotV;
    secondary_spec = saturate(secondary_spec + secondary_shift);
    secondary_spec = pow(secondary_spec, 8.0); // broader highlight

    // Coarse texture (mane hair is rough, 150x noise)
    float coarse_tex =
        fbm(v_worldPos.xz * 150.0 + vec2(v_worldPos.y * 25.0, 0.0));
    coarse_tex = coarse_tex * 0.20;

    // Mane is darker than body coat (often 15% darker)
    // Start with base color, darken significantly
    albedo = albedo * 0.75; // darker base
    albedo =
        albedo *
        (1.0 - clumping * 0.3); // clumps are darker (shadow between strands)
    albedo = albedo * (1.0 + hair_strands * 0.15); // individual hair variation
    albedo = albedo * (1.0 + coarse_tex * 0.10);   // coarse texture detail

    // Material properties for hair
    roughness = 0.65 + coarse_tex * 0.15; // coarse hair = rough
    roughness = saturate(roughness);
    F0 = vec3(0.045); // slightly higher than body coat
    metalness = 0.0;

    // Normal perturbation from hair strands (strong directional)
    float hair_bump = hair_strand_noise * 2.0 + clump_noise * 0.8;
    N = perturb_normal_ws(N, v_worldPos, hair_bump, 0.65);

    // Lighting composition
    col += ambient * albedo * 0.90; // mane in partial shadow

    // Diffuse (reduced for hair)
    col += NdotL_wrap * albedo * 0.75;

    // Anisotropic highlights (Kajiya-Kay)
    col +=
        primary_spec * vec3(0.85, 0.80, 0.75) * 0.35; // warm primary highlight
    col += secondary_spec * vec3(0.95, 0.92, 0.88) * 0.18; // softer secondary

    // Add some rim light for hair strands
    float rim = pow(1.0 - NdotV, 3.0) * 0.12;
    col += rim * vec3(0.9, 0.85, 0.80);

  } else if (is_horse_hoof) {
    // =====================================================================
    // HORSE HOOF RENDERING (Phase 2)
    // Dark horn keratin with growth rings and wear patterns
    // =====================================================================

    // Hooves are dark horn keratin (black/dark brown base)
    // Darken the base color significantly
    albedo = albedo * vec3(0.15, 0.12, 0.10); // very dark, slightly warm

    // Growth rings (hooves grow from coronet band downward)
    // Rings are ~1-2mm apart, giving ~45x frequency for ring pattern
    float ring_height = v_worldPos.y * 45.0; // vertical rings
    float growth_rings = abs(sin(ring_height)) * 0.08;
    // Rings more visible near top (fresher growth)
    growth_rings *= smoothstep(0.0, 0.15, v_bodyHeight);

    // Edge chipping (hooves wear down at bottom edge)
    // v_hoofWear = 0.0-0.3 computed in vertex shader
    float chip_noise = fbm(v_worldPos.xz * 55.0);
    float chipping = smoothstep(0.65, 0.85, chip_noise) * v_hoofWear;
    // Chips only at bottom edge (body_height < 0.05)
    chipping *= smoothstep(0.08, 0.02, v_bodyHeight);

    // Hardened keratin sheen (polished by ground contact)
    // Bottom of hoof is smoother from wear
    float ground_contact = smoothstep(0.10, 0.03, v_bodyHeight);
    float keratin_sheen =
        pow(saturate(dot(V, reflect(L, N))), 15.0) * ground_contact;
    keratin_sheen *= 0.18;

    // Dirt/mud in crevices (procedural accumulation)
    float crevice_noise =
        fbm(v_worldPos.xz * 30.0 + vec2(0.0, v_worldPos.y * 20.0));
    float crevices = smoothstep(0.45, 0.55, crevice_noise) * 0.12;
    // More dirt at bottom
    float dirt_accum = smoothstep(0.15, 0.05, v_bodyHeight) * 0.35;
    float dirt_pattern = fbm(v_worldPos.xz * 18.0);
    float dirt = dirt_accum * (0.6 + 0.4 * dirt_pattern);

    // Keratin texture (fine detail, harder than hair)
    float keratin_tex =
        fbm(v_worldPos.xz * 80.0 + vec2(v_worldPos.y * 35.0, 0.0));
    keratin_tex *= 0.08;

    // Composite albedo
    albedo = albedo * (1.0 + growth_rings); // subtle ring variation
    albedo = albedo *
             (1.0 - chipping * 0.4); // chips are darker (exposed inner hoof)
    albedo =
        mix(albedo, albedo * 1.8, dirt * 0.7); // dirt lightens (grey/brown mud)
    albedo = albedo * (1.0 + keratin_tex * 0.12); // fine texture
    albedo = albedo * (1.0 + crevices * 0.15);    // crevices slightly lighter

    // Material properties for keratin
    // Keratin is hard and can be quite smooth when polished
    roughness = 0.45 - ground_contact * 0.20 + dirt * 0.30 + chipping * 0.25;
    roughness = saturate(roughness);
    F0 = vec3(0.042); // moderate specular for keratin
    metalness = 0.0;

    // Normal perturbation from texture and wear
    float hoof_bump =
        keratin_tex * 2.0 + growth_rings * 1.5 + chip_noise * chipping * 3.0;
    N = perturb_normal_ws(N, v_worldPos, hoof_bump, 0.55);

    // Lighting composition
    col += ambient * albedo;

    // Microfacet specular (PBR)
    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnel_schlick(VdotH, F0);
    vec3 spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    col += NdotL_wrap * (albedo * (1.0 - F)) + spec * 0.75;

    // Add keratin sheen highlight
    col += keratin_sheen *
           vec3(0.95, 0.93, 0.90); // cool highlight for hard surface

  } else if (is_saddle_leather) {
    // =====================================================================
    // SADDLE LEATHER RENDERING (Phase 3)
    // Vegetable-tanned leather (4-horn Roman saddle, no stirrups)
    // =====================================================================

    // Vegetable-tanned leather base (tan/brown color)
    // Darkens with age and oil treatment
    float leather_age = v_leatherWear * 0.4; // 0.0-0.2 darkening
    albedo = albedo * vec3(0.85, 0.68, 0.52) * (1.0 - leather_age);

    // Leather grain texture (natural hide texture)
    // Fine grain at 40x frequency
    float grain_noise =
        fbm(v_worldPos.xz * 40.0 + vec2(v_worldPos.y * 12.0, 0.0));
    float grain = grain_noise * 0.18 * (1.0 + v_leatherWear * 0.5);

    // Tool marks (saddles were hand-carved and shaped)
    // Horizontal marks from scraping/smoothing
    float tool_marks = abs(sin(v_worldPos.x * 28.0 + grain_noise * 3.0)) * 0.06;
    tool_marks *= smoothstep(0.3, 0.7, fbm(v_worldPos.xz * 8.0));

    // Wear patterns (high contact areas become smooth and dark)
    // Seat area (mid-height) shows most wear
    float contact_wear = smoothstep(0.4, 0.6, v_bodyHeight) * v_leatherWear;
    contact_wear *= (0.7 + 0.3 * fbm(v_worldPos.xz * 15.0));

    // Creases and folds (leather deforms with use)
    float crease_pattern = abs(sin(v_worldPos.y * 35.0 + v_worldPos.x * 8.0));
    float creases =
        smoothstep(0.85, 0.95, crease_pattern) * v_leatherWear * 0.15;

    // Oil/wax darkening (Romans treated leather with oils)
    // More oil accumulation in crevices
    float oil_accum = fbm(v_worldPos.xz * 25.0) * 0.20;
    oil_accum *= (0.6 + v_leatherWear * 0.4);

    // Edge wear (corners and edges get lighter/rougher)
    float edge_noise = fbm(v_worldPos.xz * 50.0);
    float edges = smoothstep(0.75, 0.90, edge_noise) * 0.12;

    // Stitching hints (saddles were sewn with sinew/leather cord)
    float stitch_line =
        abs(sin(v_worldPos.x * 45.0)) * abs(sin(v_worldPos.z * 45.0));
    float stitching = smoothstep(0.92, 0.98, stitch_line) * 0.08;

    // Composite albedo
    albedo = albedo * (1.0 + grain);
    albedo = albedo * (1.0 - tool_marks * 0.5);
    albedo = albedo * (1.0 - contact_wear * 0.35); // worn areas darker
    albedo = albedo * (1.0 - creases * 0.4);
    albedo = mix(albedo, albedo * 0.70, oil_accum); // oil darkens significantly
    albedo =
        albedo * (1.0 + edges * 0.3); // edges lighter (exposed fresh leather)
    albedo = albedo * (1.0 + stitching * 0.15);

    // Material properties for leather
    // Roughness increases with wear, decreases with oil
    roughness = 0.55 + v_leatherWear * 0.25 - oil_accum * 0.20;
    roughness = saturate(roughness);
    F0 = vec3(0.038); // low specular for matte leather
    metalness = 0.0;

    // Normal perturbation from grain and creases
    float leather_bump = grain_noise * 2.0 + creases * 2.5 + tool_marks * 1.5;
    N = perturb_normal_ws(N, v_worldPos, leather_bump, 0.50);

    // Lighting composition
    col += ambient * albedo;

    // Microfacet specular (PBR)
    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnel_schlick(VdotH, F0);
    vec3 spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    col += NdotL_wrap * (albedo * (1.0 - F)) + spec * 0.65;

    // Subtle leather sheen in worn areas
    float leather_sheen = pow(1.0 - NdotV, 4.0) * contact_wear * 0.08;
    col += leather_sheen * vec3(0.90, 0.85, 0.75);

  } else if (is_bridle) {
    // =====================================================================
    // BRIDLE & REINS RENDERING (Phase 3)
    // Leather straps with bronze/brass fittings
    // =====================================================================

    // Bridle leather (thinner straps, more wear than saddle)
    // Often darker dye (brown/black)
    albedo = albedo * vec3(0.45, 0.35, 0.28) * (1.0 - v_leatherWear * 0.3);

    // Leather strap texture (narrower grain pattern)
    float strap_grain =
        fbm(v_worldPos.xz * 55.0 + vec2(v_worldPos.y * 20.0, 0.0));
    strap_grain *= 0.15;

    // Stitching (bridles are carefully sewn)
    float stitch_freq = 42.0;
    float stitch_x = abs(sin(v_worldPos.x * stitch_freq));
    float stitch_z = abs(sin(v_worldPos.z * stitch_freq));
    float stitches = max(smoothstep(0.90, 0.96, stitch_x),
                         smoothstep(0.90, 0.96, stitch_z)) *
                     0.12;

    // Wear on contact points (where straps rub against horse/metal)
    float contact_noise = fbm(v_worldPos.xz * 22.0);
    float strap_wear = contact_noise * v_leatherWear * 0.40;

    // Metal fittings (buckles, rings) - bronze/brass patina
    // Small regular patterns suggest metal hardware
    float metal_pattern = step(0.85, fbm(v_worldPos.xz * 18.0)) * 0.25;
    vec3 bronze_color = vec3(0.72, 0.55, 0.35); // warm bronze
    vec3 patina_color = vec3(0.35, 0.52, 0.42); // green patina
    float patina = fbm(v_worldPos.xz * 60.0) * 0.35;
    vec3 metal_color = mix(bronze_color, patina_color, patina);

    // Flexibility creases (straps bend and fold)
    float flex_crease = abs(sin(v_worldPos.y * 40.0 + v_worldPos.x * 15.0));
    float creasing = smoothstep(0.80, 0.92, flex_crease) * v_leatherWear * 0.18;

    // Sweat staining (bridle contacts horse constantly)
    float sweat_stain = smoothstep(0.45, 0.65, v_bodyHeight) * 0.25;
    sweat_stain *= fbm(v_worldPos.xz * 12.0);

    // Composite albedo
    albedo = albedo * (1.0 + strap_grain);
    albedo = albedo * (1.0 + stitches * 0.5); // stitching lighter (thread)
    albedo = mix(albedo, albedo * 0.65, strap_wear);  // wear darkens
    albedo = mix(albedo, metal_color, metal_pattern); // metal fittings
    albedo = albedo * (1.0 - creasing * 0.35);
    albedo = mix(albedo, albedo * 0.75, sweat_stain); // sweat darkens

    // Material properties (mix leather and metal)
    float is_metal_area = metal_pattern;
    roughness =
        mix(0.60 + v_leatherWear * 0.20, 0.35 + patina * 0.25, is_metal_area);
    roughness = saturate(roughness);
    F0 = mix(vec3(0.040), vec3(0.65), is_metal_area); // leather vs bronze
    metalness = is_metal_area * 0.8;

    // Normal perturbation from stitching and creases
    float bridle_bump = strap_grain * 2.0 + stitches * 3.0 + creasing * 2.0;
    N = perturb_normal_ws(N, v_worldPos, bridle_bump, 0.55);

    // Lighting composition
    col += ambient * albedo;

    // Microfacet specular (PBR)
    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnel_schlick(VdotH, F0);
    vec3 spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    col += NdotL_wrap * (albedo * (1.0 - F)) + spec * 0.70;

    // Metal fitting highlights
    col += is_metal_area * pow(saturate(dot(N, H)), 25.0) * 0.15 *
           vec3(0.95, 0.88, 0.75);

  } else if (is_saddle_blanket) {
    // =====================================================================
    // SADDLE BLANKET RENDERING (Phase 3)
    // Wool/linen fabric (between saddle and horse back)
    // =====================================================================

    // Fabric base (wool or linen, often undyed or red)
    // Natural colors: cream, red (expensive), brown
    vec3 fabric_base = base_color; // use base color for variety

    // Weave pattern (visible textile structure)
    // Warp and weft threads at 65x frequency
    float warp_threads = abs(sin(v_worldPos.x * 65.0));
    float weft_threads = abs(sin(v_worldPos.z * 65.0));
    float weave_pattern = (warp_threads * weft_threads) * 0.18;

    // Thread thickness variation
    float thread_noise = fbm(v_worldPos.xz * 80.0);
    float thread_var = thread_noise * 0.12;

    // Color variation (natural dye inconsistency)
    float dye_variation = fbm(v_worldPos.xz * 3.5) * 0.15;

    // Wear and pilling (fabric surface roughens with use)
    float pilling_noise = fbm(v_worldPos.xz * 45.0);
    float pilling =
        smoothstep(0.60, 0.80, pilling_noise) * v_leatherWear * 0.20;

    // Sweat staining (blanket absorbs horse sweat)
    // Darkens significantly in center (under saddle)
    float center_stain = smoothstep(0.45, 0.65, v_bodyHeight) * 0.40;
    float stain_pattern = fbm(v_worldPos.xz * 8.0);
    float sweat_stain = center_stain * (0.7 + 0.3 * stain_pattern);

    // Edge fraying (blanket edges unravel over time)
    float edge_noise = fbm(v_worldPos.xz * 55.0);
    float fraying = smoothstep(0.80, 0.95, edge_noise) * v_leatherWear * 0.15;

    // Compression marks (saddle weight creates patterns)
    float compression_pattern = smoothstep(0.50, 0.65, v_bodyHeight);
    float compression = compression_pattern * fbm(v_worldPos.xz * 20.0) * 0.12;

    // Dirt accumulation (fabric traps dirt easily)
    float dirt_pattern = fbm(v_worldPos.xz * 18.0);
    float dirt = v_leatherWear * dirt_pattern * 0.30;
    vec3 dirt_color = vec3(0.32, 0.28, 0.24);

    // Composite albedo
    albedo = fabric_base;
    albedo = albedo * (1.0 + dye_variation);
    albedo = albedo * (1.0 + weave_pattern);
    albedo = albedo * (1.0 + thread_var);
    albedo = albedo * (1.0 + pilling * 0.5);
    albedo =
        mix(albedo, albedo * 0.60, sweat_stain); // sweat darkens significantly
    albedo = albedo * (1.0 - fraying * 0.4);
    albedo = albedo * (1.0 - compression * 0.3);
    albedo = mix(albedo, dirt_color, dirt); // dirt discolors fabric

    // Material properties for fabric
    // Fabric is very matte, roughness increases with wear
    roughness = 0.75 + pilling * 0.15 + sweat_stain * 0.10;
    roughness = saturate(roughness);
    F0 = vec3(0.028); // very low specular for fabric
    metalness = 0.0;

    // Normal perturbation from weave and pilling
    float fabric_bump =
        weave_pattern * 2.5 + pilling * 2.0 + thread_noise * 1.5;
    N = perturb_normal_ws(N, v_worldPos, fabric_bump, 0.60);

    // Lighting composition
    col += ambient * albedo * 1.05; // fabric reflects ambient well

    // Diffuse dominant for fabric (minimal specular)
    col += NdotL_wrap * albedo * 0.95;

    // Very subtle specular for wet/compressed areas
    float fabric_sheen = sweat_stain * compression * 0.05;
    col += fabric_sheen * vec3(0.85, 0.82, 0.78);

  } else if (is_rider_armor) {
    // =====================================================================
    // CAVALRY ARMOR RENDERING (Phase 4)
    // Scale armor (lorica squamata) and muscle cuirass variations
    // =====================================================================

    // Determine armor type by body height and unit variation
    // Scale armor for most cavalry, muscle cuirass for elite swordsmen
    bool is_muscle_cuirass =
        (v_bodyHeight > 0.55 && v_bodyHeight < 0.75 && v_armorSheen > 0.85);

    if (is_muscle_cuirass) {
      // === MUSCLE CUIRASS (Anatomical Bronze) ===
      // Elite cavalry officers wore anatomically shaped bronze cuirasses

      // Bronze base with anatomical muscle definition
      vec3 bronze_base = vec3(0.72, 0.58, 0.42) * base_color;

      // Anatomical muscle shapes (pectorals, abs, ribs)
      // Vertical ribbing for torso muscles
      float muscle_y = v_worldPos.y * 12.0;
      float muscle_pattern = abs(sin(muscle_y)) * abs(cos(muscle_y * 0.5));
      float muscle_def = smoothstep(0.40, 0.70, muscle_pattern) * 0.20;

      // Horizontal bands for ab definition
      float ab_bands = abs(sin(v_worldPos.y * 18.0)) * 0.12;
      ab_bands *= smoothstep(0.58, 0.68, v_bodyHeight); // abs mid-torso

      // Edge definition (cuirass borders)
      float edge_noise = fbm(v_worldPos.xz * 35.0);
      float edges = smoothstep(0.80, 0.92, edge_noise) * 0.15;

      // Decorative elements (medusa heads, eagles, wreaths)
      float decor_pattern = step(0.88, fbm(v_worldPos.xz * 8.0)) * 0.18;
      decor_pattern *= smoothstep(0.60, 0.65, v_bodyHeight); // chest decoration

      // Bronze patina (green oxidation)
      float patina_pattern = fbm(v_worldPos.xz * 5.5);
      float patina = patina_pattern * 0.30;
      vec3 patina_color = vec3(0.35, 0.52, 0.42); // verdigris green

      // Battle damage (dents, scratches)
      float dent_noise = fbm(v_worldPos.xz * 12.0);
      float dents = smoothstep(0.75, 0.85, dent_noise) * 0.15;

      // Leather strapping (for fastening)
      float strap_pattern =
          abs(sin(v_worldPos.x * 8.0)) * abs(sin(v_worldPos.z * 45.0));
      float straps = smoothstep(0.92, 0.98, strap_pattern) * 0.12;
      vec3 leather_color = vec3(0.35, 0.28, 0.22);

      // Composite albedo
      albedo = bronze_base;
      albedo = albedo * (1.0 + muscle_def); // highlight muscle ridges
      albedo = albedo * (1.0 + ab_bands);
      albedo = albedo * (1.0 + edges * 0.5);
      albedo = albedo * (1.0 + decor_pattern * 0.6);
      albedo = mix(albedo, patina_color, patina);
      albedo = albedo * (1.0 - dents * 0.4);             // dents darker
      albedo = mix(albedo, leather_color, straps * 0.3); // strap visibility

      // Material properties for polished bronze
      roughness = 0.22 + patina * 0.35 + dents * 0.15;
      roughness = saturate(roughness);
      F0 = vec3(0.68); // high specular for bronze
      metalness = 0.95;

      // Normal perturbation from muscle definition
      float cuirass_bump =
          muscle_def * 3.0 + ab_bands * 2.5 + decor_pattern * 2.0;
      N = perturb_normal_ws(N, v_worldPos, cuirass_bump, 0.60);

    } else {
      // === SCALE ARMOR (Lorica Squamata) ===
      // Overlapping bronze/iron scales sewn onto fabric backing

      // Individual scale geometry (small overlapping scales)
      // Scales are ~2.5cm wide, giving ~30x frequency
      float scale_x = v_worldPos.x * 30.0;
      float scale_y = v_worldPos.y * 30.0;

      // Offset every other row for brick pattern
      float row_offset = mod(floor(scale_y), 2.0) * 0.5;
      scale_x += row_offset;

      // Individual scale shape (rounded rectangle)
      vec2 scale_uv = fract(vec2(scale_x, scale_y)) - 0.5;
      float scale_dist = max(abs(scale_uv.x) * 1.2, abs(scale_uv.y)) - 0.35;
      float scale = smoothstep(0.05, -0.05, scale_dist);

      // Overlapping pattern (scales cover each other)
      float overlap_pattern = smoothstep(0.4, 0.5, fract(scale_y));
      float overlap = scale * overlap_pattern * 0.25;

      // Scale edge lighting (rim light on scale edges)
      float scale_edge = smoothstep(-0.02, 0.02, abs(scale_dist)) * scale;
      float edge_lighting = scale_edge * pow(saturate(dot(N, L)), 2.0) * 0.35;

      // Metal variation per scale (some scales more worn/oxidized)
      float scale_variation = hash(floor(vec2(scale_x, scale_y)));
      float scale_patina = scale_variation * 0.25;

      // Bronze/iron mix (cavalry scale armor often mixed metals)
      bool is_iron_scale = (scale_variation > 0.65);
      vec3 bronze_color = vec3(0.72, 0.58, 0.42);
      vec3 iron_color = vec3(0.60, 0.60, 0.62);
      vec3 metal_color =
          mix(bronze_color, iron_color, step(0.65, scale_variation));

      // Verdigris patina on bronze scales
      vec3 patina_color = vec3(0.35, 0.52, 0.42);
      metal_color = mix(metal_color, patina_color,
                        scale_patina * (1.0 - float(is_iron_scale)));

      // Fabric backing visible between scales
      float fabric_gap = (1.0 - scale) * 0.40;
      vec3 fabric_color = vec3(0.42, 0.38, 0.35); // dark fabric

      // Battle damage (missing/bent scales)
      float damage_pattern = fbm(v_worldPos.xz * 8.0);
      float damage = smoothstep(0.82, 0.92, damage_pattern) * 0.20;

      // Composite albedo
      albedo = metal_color * scale;                   // scale metal color
      albedo = mix(albedo, fabric_color, fabric_gap); // fabric between scales
      albedo = albedo * (1.0 + overlap);      // overlapping scales brighter
      albedo = albedo * (1.0 - damage * 0.5); // damage darkens
      albedo = albedo * base_color * 0.7;     // tint with base color

      // Material properties (metal scales on fabric)
      float metal_area = scale * (1.0 - damage);
      roughness = mix(0.75, 0.32 + scale_patina * 0.30, metal_area);
      roughness = saturate(roughness);
      F0 = mix(vec3(0.030), vec3(0.65), metal_area);
      metalness = metal_area * 0.85;

      // Normal perturbation from scale geometry
      float scale_height = scale * 2.0 + overlap * 1.5;
      N = perturb_normal_ws(N, v_worldPos, scale_height, 0.70);

      // Add edge lighting to final color later
      albedo = albedo * (1.0 + edge_lighting);
    }

    // Common lighting for both armor types
    col += ambient * albedo * 0.85; // armor in partial shadow

    // Microfacet specular (PBR)
    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnel_schlick(VdotH, F0);
    vec3 spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    col += NdotL_wrap * (albedo * (1.0 - F) * 0.60 + spec * 1.25);

    // Additional metallic rim light
    float armor_rim = pow(1.0 - NdotV, 4.0) * 0.15;
    col += armor_rim * vec3(0.95, 0.90, 0.82);

  } else if (is_rider_helmet) {
    // =====================================================================
    // HELMET RENDERING (Phase 4)
    // Attic, Phrygian, and Thracian style variations
    // =====================================================================

    // Helmet style determined by height (different parts)
    bool is_crest_holder = (v_bodyHeight > 0.85); // top of helmet
    bool is_cheek_guard = (v_bodyHeight < 0.30);  // lower face protection
    bool is_bowl = (!is_crest_holder && !is_cheek_guard); // main helmet bowl

    // Bronze helmet base
    vec3 bronze_base = vec3(0.72, 0.58, 0.42) * base_color;

    if (is_bowl) {
      // === HELMET BOWL (Main Protection) ===

      // Hammered bronze texture
      float hammer_marks = fbm(v_worldPos.xz * 25.0) * 0.12;

      // Decorative ribbing (strengthen helmet structure)
      float ribbing_pattern =
          abs(sin(v_worldPos.y * 20.0 + v_worldPos.x * 5.0));
      float ribbing = smoothstep(0.88, 0.94, ribbing_pattern) * 0.15;

      // Bronze patina (oxidation)
      float patina_pattern = fbm(v_worldPos.xz * 6.0);
      float patina = patina_pattern * 0.28;
      vec3 patina_color = vec3(0.35, 0.52, 0.42);

      // Battle damage (dents from blows)
      float dent_pattern = fbm(v_worldPos.xz * 10.0);
      float dents = smoothstep(0.78, 0.88, dent_pattern) * 0.18;

      // Polished vs worn areas
      float polish_pattern = smoothstep(0.60, 0.75, v_bodyHeight);
      float polish = polish_pattern * (1.0 - patina * 0.5);

      // Composite albedo
      albedo = bronze_base;
      albedo = albedo * (1.0 + hammer_marks);
      albedo = albedo * (1.0 + ribbing * 0.6);
      albedo = mix(albedo, patina_color, patina);
      albedo = albedo * (1.0 - dents * 0.35);

      roughness = 0.25 - polish * 0.10 + patina * 0.35 + dents * 0.20;

    } else if (is_cheek_guard) {
      // === CHEEK GUARDS (Hinged Face Protection) ===

      // Thinner bronze, more flexible
      float guard_texture = fbm(v_worldPos.xz * 35.0) * 0.10;

      // Hinge rivets
      float rivet_pattern = step(0.90, fbm(v_worldPos.xz * 22.0)) * 0.15;

      // More patina (close to face, more moisture)
      float guard_patina = fbm(v_worldPos.xz * 7.0) * 0.35;
      vec3 patina_color = vec3(0.35, 0.52, 0.42);

      // Scratches from wear
      float scratches = abs(sin(v_worldPos.x * 60.0)) * 0.08;

      albedo = bronze_base;
      albedo = albedo * (1.0 + guard_texture);
      albedo = albedo * (1.0 + rivet_pattern * 0.4);
      albedo = mix(albedo, patina_color, guard_patina);
      albedo = albedo * (1.0 - scratches * 0.5);

      roughness = 0.32 + guard_patina * 0.38;

    } else { // is_crest_holder
      // === CREST HOLDER (Top Ridge) ===

      // Reinforced ridge for plume attachment
      float ridge_pattern = abs(sin(v_worldPos.x * 15.0)) * 0.18;

      // Decorative engraving
      float engraving = fbm(v_worldPos.xz * 12.0) * 0.14;

      // Less patina (exposed to air, drier)
      float top_patina = fbm(v_worldPos.xz * 5.0) * 0.20;
      vec3 patina_color = vec3(0.35, 0.52, 0.42);

      albedo = bronze_base;
      albedo = albedo * (1.0 + ridge_pattern);
      albedo = albedo * (1.0 + engraving * 0.5);
      albedo = mix(albedo, patina_color, top_patina);

      roughness = 0.28 + top_patina * 0.30;
    }

    // Common helmet properties
    roughness = saturate(roughness);
    F0 = vec3(0.66); // bronze specular
    metalness = 0.92;

    // Normal perturbation
    float helmet_bump = fbm(v_worldPos.xz * 28.0) * 2.0;
    N = perturb_normal_ws(N, v_worldPos, helmet_bump, 0.55);

    // Lighting composition
    col += ambient * albedo * 0.80;

    // Microfacet specular
    float D = D_GGX(saturate(dot(N, H)), roughness);
    float G = G_Smith(saturate(dot(N, V)), saturate(dot(N, L)), roughness);
    vec3 F = fresnel_schlick(VdotH, F0);
    vec3 spec = (D * G) * F / max(1e-5, 4.0 * NdotV * NdotL);

    col += NdotL_wrap * (albedo * (1.0 - F) * 0.55 + spec * 1.35);

    // Bronze rim light
    float helmet_rim = pow(1.0 - NdotV, 3.5) * 0.18;
    col += helmet_rim * vec3(0.95, 0.88, 0.75);

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

  } else { // leather
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

  // final clamp and alpha
  col = saturate(col);
  FragColor = vec4(col, u_alpha);
}
