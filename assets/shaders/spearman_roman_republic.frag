#version 330 core

in vec3 v_normal;
in vec3 v_worldNormal;
in vec3 v_tangent;
in vec3 v_bitangent;
in vec2 v_texCoord;
in vec3 v_worldPos;
in vec3 v_instanceColor;
in float v_instanceAlpha;
in float v_armorLayer;
in float v_bodyHeight;
in float v_helmetDetail;
in float v_steelWear;
in float v_chainmailPhase;
in float v_rivetPattern;
in float v_leatherWear;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_useTexture;
uniform float u_alpha;
uniform bool u_instanced;
uniform int u_materialId;

out vec4 FragColor;

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

float chainmail_rings(vec2 p) {
  vec2 grid = fract(p * 32.0) - 0.5;
  float ring = length(grid);
  float ring_pattern =
      smoothstep(0.38, 0.32, ring) - smoothstep(0.28, 0.22, ring);
  vec2 offset_grid = fract(p * 32.0 + vec2(0.5, 0.0)) - 0.5;
  float offset_ring = length(offset_grid);
  float offset_pattern =
      smoothstep(0.38, 0.32, offset_ring) - smoothstep(0.28, 0.22, offset_ring);
  return (ring_pattern + offset_pattern) * 0.14;
}

float pteruges_strips(vec2 p, float y) {
  float strip_x = fract(p.x * 9.0);
  float strip =
      smoothstep(0.15, 0.20, strip_x) - smoothstep(0.80, 0.85, strip_x);
  float leather_tex = noise(p * 18.0) * 0.35;
  float hang = smoothstep(0.65, 0.45, y);
  return strip * leather_tex * hang;
}

void main() {
  vec3 base_color_in = u_instanced ? v_instanceColor : u_color;
  float alpha_in = u_instanced ? v_instanceAlpha : u_alpha;
  vec3 color = base_color_in;
  if (u_useTexture) {
    color *= texture(u_texture, v_texCoord).rgb;
  }

  vec3 normal = normalize(v_normal);
  vec2 uv = v_worldPos.xz * 4.5;

  bool is_skin = (u_materialId == 0);
  bool is_armor = (u_materialId == 1);
  bool is_helmet = (u_materialId == 2);
  bool is_weapon = (u_materialId == 3);
  bool is_shield = (u_materialId == 4);
  bool is_greaves = (u_materialId == 5);

  if (is_skin) {
    float skin_detail = noise(uv * 18.0) * 0.06;
    float subdermal = noise(uv * 6.0) * 0.05;
    vec3 V = normalize(vec3(0.0, 1.0, 0.35));
    float rim =
        pow(1.0 - max(dot(normalize(v_worldNormal), V), 0.0), 4.0) * 0.04;
    color *= 1.0 + skin_detail;
    color += vec3(0.025, 0.015, 0.010) * subdermal;
    color += vec3(rim);
  } else if (is_helmet) {

    float brushed = abs(sin(v_worldPos.y * 95.0)) * 0.020;
    float dents = noise(uv * 6.5) * 0.032 * v_steelWear;
    float rust_tex = noise(uv * 9.0) * 0.11 * v_steelWear;

    float bands = v_helmetDetail * 0.12;
    float rivets = v_rivetPattern * 0.10;

    float cheek_guard_height = smoothstep(0.70, 0.80, v_bodyHeight) *
                               smoothstep(0.90, 0.80, v_bodyHeight);
    float cheek_x = abs(v_worldPos.x);
    float cheek_guard =
        cheek_guard_height * smoothstep(0.12, 0.09, cheek_x) * 0.40;
    float hinge_pins =
        cheek_guard * step(0.94, fract(v_bodyHeight * 28.0)) * 0.20;
    float guard_repousse = cheek_guard * noise(uv * 12.0) * 0.08;

    float brow_height = smoothstep(0.84, 0.88, v_bodyHeight) *
                        smoothstep(0.92, 0.88, v_bodyHeight);
    float front_facing = step(0.3, v_worldNormal.z);
    float brow_reinforce = brow_height * front_facing * 0.22;

    float plume_socket_height = smoothstep(0.90, 0.94, v_bodyHeight);
    float plume_socket =
        plume_socket_height * smoothstep(0.05, 0.03, abs(v_worldPos.x)) * 0.28;

    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float view_angle = max(dot(normalize(v_worldNormal), V), 0.0);
    float steel_sheen = pow(view_angle, 9.0) * 0.30;
    float steel_fresnel = pow(1.0 - view_angle, 2.0) * 0.22;

    color += vec3(steel_sheen + steel_fresnel + bands + rivets);
    color += vec3(cheek_guard + hinge_pins + guard_repousse);
    color += vec3(brow_reinforce + plume_socket);
    color -= vec3(rust_tex * 0.3);
    color += vec3(brushed * 0.6);
  }

  else if (is_armor) {

    color = color * vec3(0.42, 0.46, 0.50);

    vec2 chain_uv = v_worldPos.xz * 22.0;

    float chest_plate = smoothstep(1.15, 1.25, v_bodyHeight) *
                        smoothstep(1.55, 1.45, v_bodyHeight) *
                        smoothstep(0.25, 0.15, abs(v_worldPos.x));

    if (chest_plate > 0.3) {

      color = vec3(0.72, 0.76, 0.82);

      float plate_edge = smoothstep(0.88, 0.92, chest_plate) * 0.25;
      float rivets = step(0.92, fract(v_worldPos.x * 18.0)) *
                     step(0.92, fract(v_worldPos.y * 12.0)) * 0.30;

      vec3 V = normalize(vec3(0.0, 1.0, 0.5));
      float view_angle = max(dot(normalize(v_worldNormal), V), 0.0);
      float plate_sheen = pow(view_angle, 8.0) * 0.75;

      color += vec3(plate_sheen + rivets);
      color -= vec3(plate_edge);
    } else {

      float rings = chainmail_rings(chain_uv) * 2.2;
      float ring_gaps = (1.0 - chainmail_rings(chain_uv)) * 0.50;

      float ring_quality = noise(chain_uv * 3.5) * 0.18;
      float wire_thickness = noise(chain_uv * 28.0) * 0.12;
      float ring_joints = step(0.88, fract(chain_uv.x * 0.5)) *
                          step(0.88, fract(chain_uv.y * 0.5)) * 0.35;
      float gap_shadow = ring_joints * smoothstep(0.4, 0.6, rings);

      float rust_seed = noise(chain_uv * 4.2);
      float oxidation = smoothstep(0.45, 0.75, rust_seed) * v_steelWear * 0.28;
      vec3 rust_color = vec3(0.38, 0.28, 0.22);
      vec3 dark_rust = vec3(0.28, 0.20, 0.16);

      float joint_stress = smoothstep(1.20, 1.30, v_bodyHeight) *
                           smoothstep(1.00, 1.10, v_bodyHeight) * 0.25;
      float stress_wear = joint_stress * noise(chain_uv * 8.0) * 0.15;

      float damage = step(0.86, noise(chain_uv * 0.8)) * 0.40;

      vec3 V = normalize(vec3(0.0, 1.0, 0.5));
      float view_angle = max(dot(normalize(v_worldNormal), V), 0.0);
      float chain_sheen = pow(view_angle, 4.5) * 0.55;

      float shimmer =
          abs(sin(chain_uv.x * 32.0) * sin(chain_uv.y * 32.0)) * 0.22;

      color += vec3(rings + chain_sheen + shimmer);
      color -= vec3(ring_gaps + damage + gap_shadow + stress_wear);
      color *= 1.0 + ring_quality - 0.05 + wire_thickness;

      color = mix(color, rust_color, oxidation * 0.40);
      color = mix(color, dark_rust, oxidation * oxidation * 0.15);
    }

    color = clamp(color, vec3(0.32), vec3(0.88));
  }

  else if (is_shield) {

    float boss_dist = length(v_worldPos.xy);
    float boss = smoothstep(0.12, 0.08, boss_dist) * 0.6;
    float boss_rim = smoothstep(0.14, 0.12, boss_dist) *
                     smoothstep(0.10, 0.12, boss_dist) * 0.3;

    float edge_dist = max(abs(v_worldPos.x), abs(v_worldPos.y));
    float edge_wear = smoothstep(0.42, 0.48, edge_dist);
    float wood_layers = edge_wear * noise(uv * 40.0) * 0.25;

    float fabric_grain = noise(uv * 25.0) * 0.08;
    float canvas_weave = sin(uv.x * 60.0) * sin(uv.y * 58.0) * 0.04;

    float metal_edge = smoothstep(0.46, 0.48, edge_dist) * 0.45;
    vec3 bronze_edge = vec3(0.75, 0.55, 0.30);

    float dents = noise(uv * 8.0) * v_steelWear * 0.18;
    float cuts = step(0.90, noise(uv * 12.0)) * 0.28;

    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float view_angle = max(dot(normalize(v_worldNormal), V), 0.0);
    float boss_sheen = pow(view_angle, 10.0) * boss * 0.55;

    color *= 1.0 + fabric_grain + canvas_weave - 0.05;
    color += vec3(boss * 0.4 + boss_rim * 0.25 + boss_sheen);
    color += vec3(wood_layers * 0.3);
    color = mix(color, bronze_edge, metal_edge);
    color -= vec3(dents * 0.08 + cuts * 0.12);
  }

  else if (is_weapon) {

    bool is_blade = (v_bodyHeight > 0.5);
    bool is_shaft = (v_bodyHeight > 0.2 && v_bodyHeight <= 0.5);
    bool is_grip = (v_bodyHeight <= 0.2);

    if (is_blade) {

      vec3 steel_color = vec3(0.72, 0.75, 0.80);

      float fuller_x = abs(v_worldPos.x);
      float fuller = smoothstep(0.03, 0.01, fuller_x) * 0.20;

      float edge = smoothstep(0.09, 0.10, fuller_x) * 0.45;

      float forge_marks = noise(uv * 45.0) * 0.10;
      float tempering = abs(sin(v_bodyHeight * 35.0)) * 0.08;

      vec3 V = normalize(vec3(0.0, 1.0, 0.5));
      float view_angle = max(dot(normalize(v_worldNormal), V), 0.0);
      float metal_sheen = pow(view_angle, 15.0) * 0.60;

      color = mix(color, steel_color, 0.7);
      color -= vec3(fuller);
      color += vec3(edge + metal_sheen);
      color += vec3(forge_marks * 0.5 + tempering * 0.3);
    } else if (is_shaft) {

      float wood_grain = noise(uv * 32.0) * 0.16;
      float rings = abs(sin(v_worldPos.y * 40.0)) * 0.08;
      color *= vec3(0.68, 0.58, 0.45);
      color *= 1.0 + wood_grain + rings;
    } else if (is_grip) {

      float leather_wrap = noise(uv * 22.0) * 0.14;
      float binding = step(0.90, fract(v_worldPos.y * 25.0)) * 0.18;
      color *= vec3(0.48, 0.38, 0.28);
      color *= 1.0 + leather_wrap;
      color += vec3(binding);
    }
  }

  else if (is_greaves) {

    vec3 bronze_base = vec3(0.88, 0.72, 0.45);
    vec3 bronze_highlight = vec3(1.0, 0.92, 0.75);
    vec3 bronze_shadow = vec3(0.55, 0.42, 0.25);

    color = mix(color, bronze_base, 0.90);

    float brushed = abs(sin(v_worldPos.y * 120.0)) * 0.015;

    vec3 V = normalize(vec3(0.15, 0.85, 0.5));
    vec3 N = normalize(v_worldNormal);
    float NdotV = max(dot(N, V), 0.0);

    float spec_primary = pow(NdotV, 32.0) * 1.2;

    float spec_secondary = pow(NdotV, 8.0) * 0.45;

    float fresnel = pow(1.0 - NdotV, 3.0) * 0.65;

    float aniso = pow(abs(sin(v_worldPos.y * 40.0 + NdotV * 3.14)), 4.0) * 0.25;

    float env_up = max(N.y, 0.0) * 0.35;
    float env_side = (1.0 - abs(N.y)) * 0.20;

    vec3 shine = bronze_highlight * (spec_primary + spec_secondary + aniso);
    shine += vec3(fresnel * 0.8, fresnel * 0.7, fresnel * 0.5);
    shine += bronze_base * (env_up + env_side);

    color += shine;
    color += vec3(brushed);

    color = clamp(color, bronze_shadow, vec3(1.0));
  }

  color = clamp(color, 0.0, 1.0);

  float campaign_wear = (1.0 - v_bodyHeight * 0.5) * 0.18;
  float dust_accumulation = noise(v_worldPos.xz * 8.0) * campaign_wear * 0.14;

  float rain_streaks =
      smoothstep(0.85, 0.92,
                 noise(v_worldPos.xz * 2.5 + vec2(0.0, v_worldPos.y * 8.0))) *
      v_bodyHeight * 0.10;

  float mud_height = smoothstep(0.55, 0.15, v_bodyHeight);
  float mud_pattern =
      step(0.72, noise(v_worldPos.xz * 12.0 + v_worldPos.y * 3.0));
  float mud_splatter = mud_height * mud_pattern * 0.22;
  vec3 mud_color = vec3(0.22, 0.18, 0.14);

  float blood_height = smoothstep(0.85, 0.60, v_bodyHeight);
  float blood_pattern =
      step(0.88, noise(v_worldPos.xz * 15.0 + v_bodyHeight * 5.0));
  float blood_stain = blood_height * blood_pattern * v_steelWear * 0.12;
  vec3 blood_color = vec3(0.18, 0.08, 0.06);

  color -= vec3(dust_accumulation);
  color -= vec3(rain_streaks * 0.6);
  color = mix(color, mud_color, mud_splatter);
  color = mix(color, blood_color, blood_stain);

  vec3 light_dir = normalize(vec3(1.0, 1.15, 1.0));
  float n_dot_l = dot(normalize(v_worldNormal), light_dir);

  float wrap_amount =
      is_helmet ? 0.12 : (is_armor ? 0.22 : (is_greaves ? 0.12 : 0.35));
  float diff = max(n_dot_l * (1.0 - wrap_amount) + wrap_amount, 0.20);

  if (is_helmet || is_greaves) {
    diff = pow(diff, 0.88);
  }

  color *= diff;
  FragColor = vec4(color, alpha_in);
}
