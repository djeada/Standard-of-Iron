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
in float v_platePhase;
in float v_segmentStress;
in float v_rivetPattern;
in float v_polishLevel;

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
// MATERIAL PATTERN FUNCTIONS (Roman Swordsman - Legionary)
// ============================================================================

// Segmented armor plates (lorica segmentata)
float armor_plates(vec2 p, float y) {
  float plate_y = fract(y * 6.5);
  float plate_line = smoothstep(0.90, 0.98, plate_y) * 0.12;
  float rivet_x = fract(p.x * 18.0);
  float rivet =
      smoothstep(0.48, 0.50, rivet_x) * smoothstep(0.52, 0.50, rivet_x);
  float rivet_pattern = rivet * step(0.92, plate_y) * 0.25;
  return plate_line + rivet_pattern;
}

float pteruges_strips(vec2 p, float y) {
  float strip_x = fract(p.x * 9.0);
  float strip =
      smoothstep(0.15, 0.20, strip_x) - smoothstep(0.80, 0.85, strip_x);
  float leather_tex = noise(p * 18.0) * 0.35;
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
  vec2 uv = v_worldPos.xz * 5.0;

  // Material ID: 0=body/skin, 1=armor, 2=helmet, 3=weapon, 4=shield, 5=greaves
  bool is_skin = (u_materialId == 0);
  bool is_armor = (u_materialId == 1);
  bool is_helmet = (u_materialId == 2);
  bool is_weapon = (u_materialId == 3);
  bool is_shield = (u_materialId == 4);
  bool is_greaves = (u_materialId == 5);

  // === ROMAN SWORDSMAN (LEGIONARY) MATERIALS ===

  // HEAVY STEEL HELMET (galea - cool blue-grey steel)
  if (is_skin) {
    vec3 V = normalize(vec3(0.0, 1.0, 0.35));
    float skin_detail = noise(uv * 18.0) * 0.06;
    float subdermal = noise(uv * 6.0) * 0.05;
    float rim =
        pow(1.0 - max(dot(normalize(v_worldNormal), V), 0.0), 4.0) * 0.04;
    color *= 1.0 + skin_detail;
    color += vec3(0.025, 0.015, 0.010) * subdermal;
    color += vec3(rim);
  } else if (is_helmet) {
    // Polished steel finish with vertex polish level
    float brushed_metal = abs(sin(v_worldPos.y * 95.0)) * 0.02;
    float scratches = noise(uv * 35.0) * 0.018 * (1.0 - v_polishLevel * 0.5);
    float dents = noise(uv * 8.0) * 0.025;

    // Use vertex-computed helmet detail
    float bands = v_helmetDetail * 0.12;
    float rivets = v_rivetPattern * 0.12;

    // ENHANCED: Elaborate cheek guards (legionary standard)
    float cheek_guard_height = smoothstep(0.68, 0.78, v_bodyHeight) *
                               smoothstep(0.92, 0.82, v_bodyHeight);
    float cheek_x = abs(v_worldPos.x);
    float cheek_guard =
        cheek_guard_height * smoothstep(0.14, 0.10, cheek_x) * 0.45;
    float guard_edge =
        cheek_guard * smoothstep(0.36, 0.40, noise(uv * 16.0)) * 0.22;
    float hinge_detail =
        cheek_guard * step(0.92, fract(v_bodyHeight * 32.0)) * 0.25;

    // ENHANCED: Extended neck guard (deep rear protection)
    float neck_guard_height = smoothstep(0.64, 0.70, v_bodyHeight) *
                              smoothstep(0.84, 0.74, v_bodyHeight);
    float behind_head = step(v_worldNormal.z, -0.20);
    float neck_guard = neck_guard_height * behind_head * 0.38;
    float neck_lamellae =
        step(0.82, fract(v_bodyHeight * 48.0)) * neck_guard * 0.18;
    float neck_rivets =
        step(0.90, fract(v_worldPos.x * 22.0)) * neck_guard * 0.12;

    // ENHANCED: Reinforced brow band (legionary frontal ridge)
    float brow_height = smoothstep(0.82, 0.86, v_bodyHeight) *
                        smoothstep(0.94, 0.88, v_bodyHeight);
    float front_facing = step(0.4, v_worldNormal.z);
    float brow_reinforce = brow_height * front_facing * 0.28;
    float brow_decoration = brow_reinforce * noise(uv * 20.0) * 0.10;

    // ENHANCED: Plume socket with mounting bracket (officer insignia)
    float plume_socket_height = smoothstep(0.88, 0.92, v_bodyHeight);
    float plume_socket =
        plume_socket_height * smoothstep(0.06, 0.04, abs(v_worldPos.x)) * 0.32;
    float socket_bracket = plume_socket * step(0.85, noise(uv * 15.0)) * 0.15;

    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float view_angle = max(dot(normalize(v_worldNormal), V), 0.0);
    float fresnel = pow(1.0 - view_angle, 1.8) * 0.35;
    float specular = pow(view_angle, 12.0) * 0.55 * v_polishLevel;
    float sky_reflection = (v_worldNormal.y * 0.5 + 0.5) * 0.12;

    color += vec3(fresnel + sky_reflection + specular * 1.8 + bands + rivets);
    color += vec3(cheek_guard + guard_edge + hinge_detail);
    color += vec3(neck_guard + neck_lamellae + neck_rivets);
    color += vec3(brow_reinforce + brow_decoration);
    color += vec3(plume_socket + socket_bracket);
    color += vec3(brushed_metal);
    color -= vec3(scratches + dents * 0.4);
  }
  // HEAVY SEGMENTED ARMOR (lorica segmentata - iconic Roman plate armor)
  else if (is_armor) {
    // FORCE polished steel base - segmentata is BRIGHT, REFLECTIVE armor
    color = vec3(0.72, 0.78, 0.88); // Bright steel base - NOT skin tone!

    vec2 armor_uv = v_worldPos.xz * 5.5;

    // === HORIZONTAL PLATE BANDS - MUST BE OBVIOUS ===
    // 6-7 clearly visible bands wrapping torso
    float band_pattern = fract(v_platePhase);

    // STRONG band edges (plate separations)
    float band_edge = step(0.92, band_pattern) + step(band_pattern, 0.08);
    float plate_line = band_edge * 0.55; // Much stronger

    // DEEP shadows between overlapping plates
    float overlap_shadow = smoothstep(0.90, 0.98, band_pattern) * 0.65;

    // Alternating plate brightness (polishing variation)
    float plate_brightness = step(0.5, fract(v_platePhase * 0.5)) * 0.15;

    // === RIVETS - CLEARLY VISIBLE ===
    // Large brass rivets along each band edge
    float rivet_x = fract(v_worldPos.x * 16.0);
    float rivet_y = fract(v_platePhase * 6.5); // Align with bands
    float rivet = smoothstep(0.45, 0.50, rivet_x) *
                  smoothstep(0.55, 0.50, rivet_x) *
                  (step(0.92, rivet_y) + step(rivet_y, 0.08));
    float brass_rivets = rivet * 0.45;         // Much more visible
    vec3 brass_color = vec3(0.95, 0.82, 0.45); // Bright brass

    // === METALLIC FINISH ===
    // Polished steel with strong reflections
    float brushed_metal = abs(sin(v_worldPos.y * 75.0)) * 0.12;

    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float view_angle = max(dot(normalize(v_worldNormal), V), 0.0);

    // VERY STRONG specular - legionary armor was highly polished
    float plate_specular = pow(view_angle, 9.0) * 0.85 * v_polishLevel;

    // Metallic fresnel
    float plate_fresnel = pow(1.0 - view_angle, 2.2) * 0.45;

    // Sky reflection
    float sky_reflect = (v_worldNormal.y * 0.5 + 0.5) * 0.35 * v_polishLevel;

    // === WEAR & DAMAGE ===
    // Battle scratches
    float scratches =
        noise(armor_uv * 42.0) * 0.08 * (1.0 - v_polishLevel * 0.7);

    // Impact dents (front armor takes hits)
    float front_facing = smoothstep(-0.2, 0.7, v_worldNormal.z);
    float dents = noise(armor_uv * 6.0) * 0.12 * front_facing;

    // ENHANCED: Joint wear between plates with articulation stress
    float joint_wear = v_segmentStress * 0.25;

    // ENHANCED: Plate edge wear (horizontal band edges show most damage)
    float edge_wear = smoothstep(0.88, 0.96, band_pattern) *
                      smoothstep(0.12, 0.04, band_pattern) * 0.18;
    float edge_darkening = edge_wear * noise(armor_uv * 15.0) * 0.22;

    // ENHANCED: Rivet stress patterns (rivets loosen/oxidize at stress points)
    float rivet_stress = rivet * v_segmentStress * 0.15;
    vec3 rivet_oxidation = vec3(0.35, 0.28, 0.20) * rivet_stress;

    // ENHANCED: Plate segment definition (each band slightly different
    // polish/color)
    float segment_variation = noise(vec2(v_platePhase, 0.5) * 2.0) * 0.08;

    // ENHANCED: Overlapping plate shadows with depth (more realistic
    // articulation)
    float plate_depth = smoothstep(0.94, 0.98, band_pattern) * 0.40;
    float gap_shadow = plate_depth * (1.0 - view_angle * 0.5); // Deeper in gaps

    // Apply all plate effects - STRONG VISIBILITY
    color += vec3(plate_brightness + plate_specular + plate_fresnel +
                  sky_reflect + brushed_metal + segment_variation);
    color -= vec3(plate_line * 0.4 + overlap_shadow + scratches + dents * 0.5 +
                  joint_wear + edge_darkening + gap_shadow);

    // Add brass rivets with color and oxidation
    color = mix(color, brass_color, brass_rivets);
    color -= rivet_oxidation;

    // Ensure segmentata is ALWAYS bright and visible
    color = clamp(color, vec3(0.45), vec3(0.95));
  }
  // SCUTUM SHIELD (curved laminated wood with metal boss)
  else if (is_shield) {
    // Shield boss (raised metal dome)
    float boss_dist = length(v_worldPos.xy);
    float boss = smoothstep(0.12, 0.08, boss_dist) * 0.6;
    float boss_rim = smoothstep(0.14, 0.12, boss_dist) *
                     smoothstep(0.10, 0.12, boss_dist) * 0.3;

    // Laminated wood construction
    float edge_dist = max(abs(v_worldPos.x), abs(v_worldPos.y));
    float edge_wear = smoothstep(0.42, 0.48, edge_dist);
    float wood_layers = edge_wear * noise(uv * 40.0) * 0.25;

    // Painted canvas facing
    float fabric_grain = noise(uv * 25.0) * 0.08;
    float canvas_weave = sin(uv.x * 60.0) * sin(uv.y * 58.0) * 0.04;

    // Bronze edging (perimeter reinforcement)
    float metal_edge = smoothstep(0.46, 0.48, edge_dist) * 0.45;
    vec3 bronze_edge = vec3(0.75, 0.55, 0.30);

    // Combat damage (more on legionary shields)
    float dents = noise(uv * 8.0) * v_polishLevel * 0.20;
    float cuts = step(0.88, noise(uv * 12.0)) * 0.32;
    float scratches = noise(uv * 50.0) * 0.15;

    // Boss polish and reflection
    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float view_angle = max(dot(normalize(v_worldNormal), V), 0.0);
    float boss_sheen = pow(view_angle, 12.0) * boss * v_polishLevel * 0.60;

    color *= 1.0 + fabric_grain + canvas_weave - 0.05;
    color += vec3(boss * 0.4 + boss_rim * 0.25 + boss_sheen);
    color += vec3(wood_layers * 0.3);
    color = mix(color, bronze_edge, metal_edge);
    color -= vec3(dents * 0.08 + cuts * 0.12 + scratches * 0.05);
  }
  // GLADIUS (Roman short sword - steel blade)
  else if (is_weapon) {
    // Weapon parts by body height
    bool is_blade = (v_bodyHeight > 0.35); // Blade section
    bool is_grip = (v_bodyHeight > 0.15 && v_bodyHeight <= 0.35); // Handle
    bool is_pommel = (v_bodyHeight <= 0.15); // Pommel/guard

    if (is_blade) {
      // Polished steel blade
      vec3 steel_color = vec3(0.78, 0.81, 0.85);

      // Fuller groove (weight reduction channel)
      float fuller_x = abs(v_worldPos.x);
      float fuller = smoothstep(0.025, 0.015, fuller_x) * 0.25;

      // Razor-sharp edge
      float edge = smoothstep(0.08, 0.10, fuller_x) * 0.50;

      // High polish finish (legionary standard)
      float polish_marks = noise(uv * 60.0) * 0.08 * v_polishLevel;

      // Edge tempering line (heat treatment)
      float temper_line = smoothstep(0.075, 0.085, fuller_x) * 0.12;

      vec3 V = normalize(vec3(0.0, 1.0, 0.5));
      float view_angle = max(dot(normalize(v_worldNormal), V), 0.0);
      float metal_sheen = pow(view_angle, 18.0) * v_polishLevel * 0.70;
      float reflection = pow(view_angle, 8.0) * 0.35;

      color = mix(color, steel_color, 0.8);
      color -= vec3(fuller);
      color += vec3(edge + metal_sheen + reflection);
      color += vec3(polish_marks * 0.6 + temper_line * 0.4);
    } else if (is_grip) {
      // Wood grip with leather wrap
      float wood_base = noise(uv * 28.0) * 0.14;
      float leather_wrap = noise(uv * 35.0) * 0.12;
      float wire_binding = step(0.88, fract(v_worldPos.y * 30.0)) * 0.20;

      color *= vec3(0.52, 0.42, 0.32); // Leather-wrapped wood
      color *= 1.0 + wood_base + leather_wrap;
      color += vec3(wire_binding * 0.5); // Bronze wire
    } else if (is_pommel) {
      // Brass pommel (counterweight and decoration)
      vec3 brass_color = vec3(0.82, 0.72, 0.42);

      float decoration = noise(uv * 20.0) * 0.10;

      vec3 V = normalize(vec3(0.0, 1.0, 0.5));
      float view_angle = max(dot(normalize(v_worldNormal), V), 0.0);
      float brass_sheen = pow(view_angle, 12.0) * 0.50;

      color = mix(color, brass_color, 0.8);
      color += vec3(brass_sheen + decoration * 0.4);
    }
  }
  // BRONZE GREAVES (shin guards - highly polished mirror-like bronze)
  else if (is_greaves) {
    // Rich polished bronze base (warm golden metallic)
    vec3 bronze_base = vec3(0.88, 0.72, 0.45);
    vec3 bronze_highlight = vec3(1.0, 0.92, 0.75);
    vec3 bronze_shadow = vec3(0.55, 0.42, 0.25);
    
    // Apply bronze base
    color = mix(color, bronze_base, 0.90);
    
    // Micro brushed metal texture (fine polish lines)
    float brushed = abs(sin(v_worldPos.y * 120.0)) * 0.015;
    
    // View direction for specular
    vec3 V = normalize(vec3(0.15, 0.85, 0.5));
    vec3 N = normalize(v_worldNormal);
    float NdotV = max(dot(N, V), 0.0);
    
    // Primary specular (sharp highlight - polished metal)
    float spec_primary = pow(NdotV, 32.0) * 1.2;
    
    // Secondary specular (broader shine)
    float spec_secondary = pow(NdotV, 8.0) * 0.45;
    
    // Metallic fresnel (bright edges)
    float fresnel = pow(1.0 - NdotV, 3.0) * 0.65;
    
    // Anisotropic highlight (stretched along greave height)
    float aniso = pow(abs(sin(v_worldPos.y * 40.0 + NdotV * 3.14)), 4.0) * 0.25;
    
    // Environment reflection (sky above, ground below)
    float env_up = max(N.y, 0.0) * 0.35;
    float env_side = (1.0 - abs(N.y)) * 0.20;
    
    // Combine all shine effects
    vec3 shine = bronze_highlight * (spec_primary + spec_secondary + aniso);
    shine += vec3(fresnel * 0.8, fresnel * 0.7, fresnel * 0.5);
    shine += bronze_base * (env_up + env_side);
    
    color += shine;
    color += vec3(brushed);
    
    // Ensure bright metallic finish
    color = clamp(color, bronze_shadow, vec3(1.0));
  }

  color = clamp(color, 0.0, 1.0);

  // === PHASE 4: ADVANCED FEATURES ===
  // Environmental interactions (elite legionary maintains higher standards)

  // Limited campaign wear (legionaries keep equipment polished)
  float campaign_wear =
      (1.0 - v_bodyHeight * 0.7) * (1.0 - v_polishLevel) * 0.12;
  float dust_accumulation = noise(v_worldPos.xz * 8.0) * campaign_wear * 0.08;

  // Rain streaks (even polished armor shows weather)
  float rain_streaks =
      smoothstep(0.87, 0.93,
                 noise(v_worldPos.xz * 2.5 + vec2(0.0, v_worldPos.y * 8.0))) *
      v_bodyHeight * 0.08; // Subtle on segmentata

  // Minimal mud (legionaries in formation, not skirmishing)
  float mud_height = smoothstep(0.45, 0.10, v_bodyHeight); // Only ankles/feet
  float mud_pattern =
      step(0.80, noise(v_worldPos.xz * 12.0 + v_worldPos.y * 3.0));
  float mud_splatter = mud_height * mud_pattern * 0.15;
  vec3 mud_color = vec3(0.22, 0.18, 0.14);

  // Blood evidence (frontline combat)
  float blood_height = smoothstep(0.88, 0.65, v_bodyHeight); // Upper torso/arms
  float blood_pattern =
      step(0.90, noise(v_worldPos.xz * 15.0 + v_bodyHeight * 5.0));
  float blood_stain =
      blood_height * blood_pattern * (1.0 - v_polishLevel * 0.5) * 0.10;
  vec3 blood_color = vec3(0.18, 0.08, 0.06);

  // Apply environmental effects (moderated by polish level)
  color -= vec3(dust_accumulation);
  color -= vec3(rain_streaks * 0.5);
  color = mix(color, mud_color, mud_splatter);
  color = mix(color, blood_color, blood_stain);

  // Subtle ambient occlusion to ground the metal and leather
  float ao = 0.90 - noise(v_worldPos.xz * 4.0) * 0.10;
  color *= ao;
  color = mix(color, vec3(dot(color, vec3(0.333))), 0.08); // mild desaturation

  // Lighting per material
  vec3 light_dir = normalize(vec3(1.0, 1.2, 1.0));
  float n_dot_l = dot(normalize(v_worldNormal), light_dir);

  float wrap_amount = is_helmet ? 0.08 : (is_armor ? 0.08 : (is_greaves ? 0.08 : 0.30));
  float diff = max(n_dot_l * (1.0 - wrap_amount) + wrap_amount, 0.16);

  // Extra contrast for polished steel and bronze
  if (is_helmet || is_armor || is_greaves) {
    diff = pow(diff, 0.85);
  }

  color *= diff;
  FragColor = vec4(color, u_alpha);
}
