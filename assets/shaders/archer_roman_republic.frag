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
in float v_armorLayer;
in float v_bodyHeight;
in float v_helmetDetail;
in float v_chainmailPhase;
in float v_leatherWear;
in float v_curvature;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_useTexture;
uniform float u_alpha;
uniform int u_materialId;

out vec4 FragColor;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

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

// ============================================================================
// MATERIAL PATTERN FUNCTIONS (Roman Archer - Sagittarius)
// ============================================================================

// Roman chainmail (lorica hamata) ring pattern
float chainmail_rings(vec2 p) {
  vec2 grid = fract(p * 32.0) - 0.5;
  float ring = length(grid);
  float ring_pattern =
      smoothstep(0.38, 0.32, ring) - smoothstep(0.28, 0.22, ring);

  // Offset rows for interlocking
  vec2 offset_grid = fract(p * 32.0 + vec2(0.5, 0.0)) - 0.5;
  float offset_ring = length(offset_grid);
  float offset_pattern =
      smoothstep(0.38, 0.32, offset_ring) - smoothstep(0.28, 0.22, offset_ring);

  return (ring_pattern + offset_pattern) * 0.14;
}

// Leather pteruges strips (hanging skirt/shoulder guards)
float pteruges_strips(vec2 p, float y) {
  // Vertical leather strips
  float strip_x = fract(p.x * 9.0);
  float strip =
      smoothstep(0.15, 0.20, strip_x) - smoothstep(0.80, 0.85, strip_x);

  // Add leather texture to strips
  float leather_tex = noise(p * 18.0) * 0.35;

  // Strips hang and curve
  float hang = smoothstep(0.65, 0.45, y);

  return strip * leather_tex * hang;
}

// ============================================================================
// MAIN FRAGMENT SHADER
// ============================================================================

void main() {
  vec3 color = u_color;
  if (u_useTexture) {
    color *= texture(u_texture, v_texCoord).rgb;
  }

  vec3 normal = normalize(v_normal);
  vec2 uv = v_worldPos.xz * 4.5;

  // Material ID: 0=body/skin, 1=armor, 2=helmet, 3=weapon, 4=shield
  bool is_armor = (u_materialId == 1);
  bool is_helmet = (u_materialId == 2);
  bool is_weapon = (u_materialId == 3);
  bool is_shield = (u_materialId == 4);

  // Use material IDs exclusively (no fallbacks)
  bool is_legs = (u_materialId == 0); // Body mesh includes legs

  // === ROMAN ARCHER (SAGITTARIUS) MATERIALS ===

  // LIGHT BRONZE HELMET (warm golden auxiliary helmet)
  if (is_helmet) {
    // Use vertex-computed helmet detail
    float bands = v_helmetDetail * 0.15;

    // Warm bronze patina and wear
    float bronze_patina = noise(uv * 8.0) * 0.12;
    float verdigris = noise(uv * 15.0) * 0.08;

    // Hammer marks from forging (using vertex curvature)
    float hammer_marks = noise(uv * 25.0) * 0.035 * (1.0 - v_curvature * 0.3);

    // Conical shape highlight
    float apex = smoothstep(0.85, 1.0, v_bodyHeight) * 0.12;

    // ENHANCED: Cheek guards (hinged bronze plates)
    float cheek_guard_height = smoothstep(0.72, 0.82, v_bodyHeight) *
                               smoothstep(0.88, 0.82, v_bodyHeight);
    float cheek_x = abs(v_worldPos.x);
    float cheek_guard =
        cheek_guard_height * smoothstep(0.10, 0.08, cheek_x) * 0.35;
    float guard_edge = cheek_guard * step(0.32, noise(uv * 18.0)) * 0.18;

    // ENHANCED: Neck guard (rear projection)
    float neck_guard_height = smoothstep(0.68, 0.74, v_bodyHeight) *
                              smoothstep(0.80, 0.74, v_bodyHeight);
    float behind_head = step(v_worldNormal.z, -0.3); // Rear-facing normals
    float neck_guard = neck_guard_height * behind_head * 0.28;
    float neck_segments = fract(v_bodyHeight * 35.0) * neck_guard * 0.12;

    // ENHANCED: Bronze composition variation (copper/tin ratio affects color)
    float bronze_variation = noise(uv * 5.0) * 0.10;
    vec3 rich_bronze = vec3(0.82, 0.68, 0.42); // Higher copper
    vec3 pale_bronze = vec3(0.75, 0.72, 0.55); // Higher tin

    // ENHANCED: Plume socket (top center mounting point)
    float plume_socket_height = smoothstep(0.92, 0.96, v_bodyHeight);
    float plume_socket =
        plume_socket_height * smoothstep(0.04, 0.02, abs(v_worldPos.x)) * 0.25;

    // Bronze sheen using tangent space
    vec3 N = normalize(v_worldNormal);
    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float view_angle = max(dot(N, V), 0.0);
    float bronze_sheen = pow(view_angle, 7.0) * 0.25;
    float bronze_fresnel = pow(1.0 - view_angle, 2.2) * 0.18;

    color = mix(color, rich_bronze, 0.6);
    color = mix(color, pale_bronze, bronze_variation);
    color += vec3(bronze_sheen + bronze_fresnel + bands + apex);
    color += vec3(cheek_guard + guard_edge + neck_guard + neck_segments +
                  plume_socket);
    color -= vec3(bronze_patina * 0.4 + verdigris * 0.3);
    color += vec3(hammer_marks * 0.5);
  }
  // LIGHT CHAINMAIL ARMOR (lorica hamata - historically accurate 4-in-1
  // pattern)
  else if (is_armor) {
    // DRAMATICALLY VISIBLE chainmail - base color much darker than body
    // Start with strong grey base that's clearly NOT skin
    color = color * vec3(0.45, 0.48, 0.52); // Force grey metal base

    // Roman chainmail: butted iron rings, 8-10mm diameter, 1.2mm wire
    vec2 chain_uv = v_worldPos.xz * 22.0; // Larger, more visible rings

    // MUCH STRONGER ring pattern - these need to be OBVIOUS
    float rings = chainmail_rings(chain_uv) * 2.5; // 3x stronger

    // Deep shadows in ring gaps - CRITICAL for visibility
    float ring_gaps = (1.0 - chainmail_rings(chain_uv)) * 0.45;

    // Ring structure with STRONG contrast
    float ring_highlight = rings * 0.85;

    // ENHANCED: Ring quality variation (handmade vs forge-produced)
    float ring_quality =
        noise(chain_uv * 3.5) * 0.18; // Size/shape inconsistency
    float wire_thickness =
        noise(chain_uv * 28.0) * 0.12; // Wire gauge variation

    // ENHANCED: Micro-gaps between interlocked rings (4-in-1 pattern realism)
    float ring_joints = step(0.88, fract(chain_uv.x * 0.5)) *
                        step(0.88, fract(chain_uv.y * 0.5)) * 0.35;
    float gap_shadow = ring_joints * smoothstep(0.4, 0.6, rings);

    // ENHANCED: Oxidation gradients (rust spreads from contact points)
    float rust_seed = noise(chain_uv * 4.2);
    float oxidation =
        smoothstep(0.45, 0.75, rust_seed) * v_chainmailPhase * 0.32;
    vec3 rust_color = vec3(0.42, 0.32, 0.26);
    vec3 dark_rust = vec3(0.28, 0.20, 0.16);

    // ENHANCED: Stress points at shoulder/elbow joints
    float joint_stress = smoothstep(1.25, 1.35, v_bodyHeight) * // Shoulders
                         smoothstep(1.05, 1.15, v_bodyHeight) * // Elbows
                         0.25;
    float stress_wear = joint_stress * noise(chain_uv * 8.0) * 0.15;

    // Iron oxidation (non-galvanized iron ages naturally)
    float iron_tarnish = noise(chain_uv * 12.0) * 0.20 * v_chainmailPhase;

    // Battle damage - visible broken rings
    float damage = step(0.88, noise(chain_uv * 0.8)) * 0.35;

    // STRONG specular highlights on ring surfaces
    vec3 N = normalize(v_worldNormal);
    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float view_angle = max(dot(N, V), 0.0);
    float chain_sheen = pow(view_angle, 4.0) * 0.65; // Much stronger

    // Metallic shimmer as rings catch light
    float shimmer = abs(sin(chain_uv.x * 32.0) * sin(chain_uv.y * 32.0)) * 0.25;

    // Apply all chainmail effects with STRONG visibility
    color += vec3(ring_highlight + chain_sheen + shimmer);
    color -= vec3(ring_gaps + damage + gap_shadow + stress_wear);
    color *= 1.0 + ring_quality - 0.05 + wire_thickness;
    color += vec3(iron_tarnish * 0.4);

    // ENHANCED: Multi-stage oxidation (gradient from bright iron to dark rust)
    color = mix(color, rust_color, oxidation * 0.35);
    color = mix(color, dark_rust,
                oxidation * oxidation * 0.15); // Deep rust in crevices

    // Ensure chainmail is CLEARLY visible - never blend into skin
    color = clamp(color, vec3(0.35), vec3(0.85));
  }
  // LEATHER PTERUGES & BELT (tan/brown leather strips)
  else if (is_legs) {
    // Thick leather with visible grain (using vertex wear data)
    float leather_grain = noise(uv * 10.0) * 0.16 * (0.5 + v_leatherWear * 0.5);
    float leather_pores = noise(uv * 22.0) * 0.08;

    // Pteruges strip pattern
    float strips = pteruges_strips(v_worldPos.xz, v_bodyHeight);

    // Worn leather edges
    float wear = noise(uv * 4.0) * v_leatherWear * 0.10 - 0.05;

    // Leather has subtle sheen
    vec3 N = normalize(v_worldNormal);
    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float view_angle = max(dot(N, V), 0.0);
    float leather_sheen = pow(1.0 - view_angle, 4.5) * 0.10;

    color *= 1.0 + leather_grain + leather_pores - 0.08 + wear;
    color += vec3(strips * 0.15 + leather_sheen);
  }
  // SCUTUM SHIELD (curved laminated wood with metal boss)
  else if (is_shield) {
    // Shield boss (metal center dome for deflection)
    float boss_dist = length(v_worldPos.xy);
    float boss = smoothstep(0.12, 0.08, boss_dist) * 0.6;
    float boss_rim = smoothstep(0.14, 0.12, boss_dist) *
                     smoothstep(0.10, 0.12, boss_dist) * 0.3;

    // Laminated wood layers (3-ply construction visible at edges)
    float edge_dist = max(abs(v_worldPos.x), abs(v_worldPos.y));
    float edge_wear = smoothstep(0.42, 0.48, edge_dist);
    float wood_layers = edge_wear * noise(uv * 40.0) * 0.25;

    // Leather/linen facing (painted surface)
    float fabric_grain = noise(uv * 25.0) * 0.08;
    float canvas_weave = sin(uv.x * 60.0) * sin(uv.y * 58.0) * 0.04;

    // Metal edging (bronze trim around perimeter)
    float metal_edge = smoothstep(0.46, 0.48, edge_dist) * 0.45;
    vec3 bronze_edge = vec3(0.75, 0.55, 0.30);

    // Battle damage (dents, cuts, scratches)
    float dents = noise(uv * 8.0) * 0.15;
    float cuts = step(0.92, noise(uv * 12.0)) * 0.25;

    // Boss metallic sheen (polished bronze)
    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float view_angle = max(dot(normalize(v_worldNormal), V), 0.0);
    float boss_sheen = pow(view_angle, 10.0) * boss * 0.55;

    color *= 1.0 + fabric_grain + canvas_weave - 0.05;
    color += vec3(boss * 0.4 + boss_rim * 0.25 + boss_sheen);
    color += vec3(wood_layers * 0.3);
    color = mix(color, bronze_edge, metal_edge);
    color -= vec3(dents * 0.08 + cuts * 0.15);
  }
  // BOW & ARROWS (composite wood, sinew, horn)
  else if (is_weapon) {
    // Composite bow construction (wood core, horn belly, sinew back)
    bool is_bow_limb = (v_bodyHeight > 0.3 && v_bodyHeight < 0.9);
    bool is_grip = (v_bodyHeight >= 0.4 && v_bodyHeight <= 0.6);
    bool is_string = (v_bodyHeight < 0.05 || v_bodyHeight > 0.95);

    if (is_bow_limb) {
      // Wood grain with horn lamination
      float wood_grain = noise(uv * 35.0) * 0.18;
      float horn_banding = abs(sin(v_bodyHeight * 25.0)) * 0.12;

      vec3 V = normalize(vec3(0.0, 1.0, 0.4));
      float view_angle = max(dot(normalize(v_worldNormal), V), 0.0);
      float horn_sheen = pow(1.0 - view_angle, 6.0) * 0.15;

      color *= 1.0 + wood_grain + horn_banding;
      color += vec3(horn_sheen);
    } else if (is_grip) {
      // Leather wrap with cord binding
      float leather_wrap = noise(uv * 20.0) * 0.14;
      float cord_binding = step(0.92, fract(v_worldPos.y * 30.0)) * 0.15;
      color *= vec3(0.55, 0.42, 0.32); // Dark leather
      color *= 1.0 + leather_wrap;
      color += vec3(cord_binding);
    } else if (is_string) {
      // Sinew bowstring (natural fiber)
      color = vec3(0.72, 0.68, 0.60); // Natural sinew color
      float fiber_twist = noise(uv * 80.0) * 0.10;
      color *= 1.0 + fiber_twist;
    }
  }

  color = clamp(color, 0.0, 1.0);

  // === PHASE 4: ADVANCED FEATURES ===
  // Environmental interactions and battle wear (procedural, no new uniforms
  // needed)

  // Battle-hardened appearance (more wear on lower body parts)
  float campaign_wear = (1.0 - v_bodyHeight * 0.6) * 0.15;
  float dust_accumulation = noise(v_worldPos.xz * 8.0) * campaign_wear * 0.12;

  // Rain streaks (vertical weathering patterns)
  float rain_streaks =
      smoothstep(0.85, 0.92,
                 noise(v_worldPos.xz * 2.5 + vec2(0.0, v_worldPos.y * 8.0))) *
      v_bodyHeight * 0.08; // More visible on upper body

  // Mud splatter (lower body, procedural based on position)
  float mud_height =
      smoothstep(0.5, 0.2, v_bodyHeight); // Concentrated at feet/legs
  float mud_pattern =
      step(0.75, noise(v_worldPos.xz * 12.0 + v_worldPos.y * 3.0));
  float mud_splatter = mud_height * mud_pattern * 0.18;
  vec3 mud_color = vec3(0.22, 0.18, 0.14);

  // Apply environmental effects
  color -= vec3(dust_accumulation);            // Dust darkens surfaces
  color -= vec3(rain_streaks * 0.5);           // Rain creates dark streaks
  color = mix(color, mud_color, mud_splatter); // Mud obscures material

  // Lighting model per material
  vec3 light_dir = normalize(vec3(1.0, 1.15, 1.0));
  float n_dot_l = dot(normal, light_dir);

  float wrap_amount = is_helmet ? 0.15 : (is_armor ? 0.22 : 0.38);
  float diff = max(n_dot_l * (1.0 - wrap_amount) + wrap_amount, 0.22);

  if (is_helmet) {
    diff = pow(diff, 0.90);
  }

  color *= diff;
  FragColor = vec4(color, u_alpha);
}
