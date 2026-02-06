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
in float v_chainmailPhase;
in float v_leatherWear;
in float v_curvature;

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
  bool is_cloak = (u_materialId == 5 || u_materialId == 6);
  bool is_greaves = (u_materialId == 5);

  if (is_skin) {
    vec3 N = normalize(v_worldNormal);
    vec3 V = normalize(vec3(0.0, 1.0, 0.35));
    float skin_detail = noise(v_worldPos.xz * 18.0) * 0.06;
    float subdermal = noise(v_worldPos.xz * 6.0) * 0.05;
    color *= 1.0 + skin_detail;
    color += vec3(0.025, 0.015, 0.010) * subdermal;
    float rim = pow(1.0 - max(dot(N, V), 0.0), 4.0) * 0.04;
    color += vec3(rim);
  } else if (is_helmet) {

    float bands = v_helmetDetail * 0.15;

    float bronze_patina = noise(uv * 8.0) * 0.12;
    float verdigris = noise(uv * 15.0) * 0.08;

    float hammer_marks = noise(uv * 25.0) * 0.035 * (1.0 - v_curvature * 0.3);

    float apex = smoothstep(0.85, 1.0, v_bodyHeight) * 0.12;

    float cheek_guard_height = smoothstep(0.72, 0.82, v_bodyHeight) *
                               smoothstep(0.88, 0.82, v_bodyHeight);
    float cheek_x = abs(v_worldPos.x);
    float cheek_guard =
        cheek_guard_height * smoothstep(0.10, 0.08, cheek_x) * 0.35;
    float guard_edge = cheek_guard * step(0.32, noise(uv * 18.0)) * 0.18;

    float bronze_variation = noise(uv * 5.0) * 0.10;
    vec3 rich_bronze = vec3(0.82, 0.68, 0.42);
    vec3 pale_bronze = vec3(0.75, 0.72, 0.55);

    float plume_socket_height = smoothstep(0.92, 0.96, v_bodyHeight);
    float plume_socket =
        plume_socket_height * smoothstep(0.04, 0.02, abs(v_worldPos.x)) * 0.25;

    vec3 N = normalize(v_worldNormal);
    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float view_angle = max(dot(N, V), 0.0);
    float bronze_sheen = pow(view_angle, 7.0) * 0.25;
    float bronze_fresnel = pow(1.0 - view_angle, 2.2) * 0.18;

    color = mix(color, rich_bronze, 0.6);
    color = mix(color, pale_bronze, bronze_variation);
    color += vec3(bronze_sheen + bronze_fresnel + bands + apex);
    color += vec3(cheek_guard + guard_edge + plume_socket);
    color -= vec3(bronze_patina * 0.4 + verdigris * 0.3);
    color += vec3(hammer_marks * 0.5);
  }

  else if (is_armor) {

    color = color * vec3(0.45, 0.48, 0.52);

    vec2 chain_uv = v_worldPos.xz * 22.0;

    float rings = chainmail_rings(chain_uv) * 2.5;

    float ring_gaps = (1.0 - chainmail_rings(chain_uv)) * 0.45;

    float ring_highlight = rings * 0.85;

    float ring_quality = noise(chain_uv * 3.5) * 0.18;
    float wire_thickness = noise(chain_uv * 28.0) * 0.12;

    float ring_joints = step(0.88, fract(chain_uv.x * 0.5)) *
                        step(0.88, fract(chain_uv.y * 0.5)) * 0.35;
    float gap_shadow = ring_joints * smoothstep(0.4, 0.6, rings);

    float rust_seed = noise(chain_uv * 4.2);
    float oxidation =
        smoothstep(0.45, 0.75, rust_seed) * v_chainmailPhase * 0.32;
    vec3 rust_color = vec3(0.42, 0.32, 0.26);
    vec3 dark_rust = vec3(0.28, 0.20, 0.16);

    float joint_stress = smoothstep(1.25, 1.35, v_bodyHeight) *
                         smoothstep(1.05, 1.15, v_bodyHeight) * 0.25;
    float stress_wear = joint_stress * noise(chain_uv * 8.0) * 0.15;

    float iron_tarnish = noise(chain_uv * 12.0) * 0.20 * v_chainmailPhase;

    float damage = step(0.88, noise(chain_uv * 0.8)) * 0.35;

    vec3 N = normalize(v_worldNormal);
    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float view_angle = max(dot(N, V), 0.0);
    float chain_sheen = pow(view_angle, 4.0) * 0.65;

    float shimmer = abs(sin(chain_uv.x * 32.0) * sin(chain_uv.y * 32.0)) * 0.25;

    color += vec3(ring_highlight + chain_sheen + shimmer);
    color -= vec3(ring_gaps + damage + gap_shadow + stress_wear);
    color *= 1.0 + ring_quality - 0.05 + wire_thickness;
    color += vec3(iron_tarnish * 0.4);

    color = mix(color, rust_color, oxidation * 0.35);
    color = mix(color, dark_rust, oxidation * oxidation * 0.15);

    color = clamp(color, vec3(0.35), vec3(0.85));
  }

  else if (is_cloak) {
    vec3 N = normalize(v_worldNormal);
    vec3 V = normalize(vec3(0.0, 1.0, 0.35));

    vec3 team_tint = clamp(base_color_in, 0.0, 1.0);
    color = mix(color, team_tint, 0.75);

    float weave = sin(v_worldPos.x * 70.0) * sin(v_worldPos.z * 70.0) * 0.04;
    float wrinkle = noise(v_worldPos.xz * 12.0) * 0.12;
    float shading = 0.65 + noise(v_worldPos.xz * 2.5) * 0.25;
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 4.0) * 0.12;

    color *= shading + weave * 0.2;
    color += vec3(wrinkle * 0.12 + fresnel);
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

    float dents = noise(uv * 8.0) * 0.15;
    float cuts = step(0.92, noise(uv * 12.0)) * 0.25;

    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    float view_angle = max(dot(normalize(v_worldNormal), V), 0.0);
    float boss_sheen = pow(view_angle, 10.0) * boss * 0.55;

    color *= 1.0 + fabric_grain + canvas_weave - 0.05;
    color += vec3(boss * 0.4 + boss_rim * 0.25 + boss_sheen);
    color += vec3(wood_layers * 0.3);
    color = mix(color, bronze_edge, metal_edge);
    color -= vec3(dents * 0.08 + cuts * 0.15);
  }

  else if (is_weapon) {

    bool is_bow_limb = (v_bodyHeight > 0.3 && v_bodyHeight < 0.9);
    bool is_grip = (v_bodyHeight >= 0.4 && v_bodyHeight <= 0.6);
    bool is_string = (v_bodyHeight < 0.05 || v_bodyHeight > 0.95);

    if (is_bow_limb) {

      float wood_grain = noise(uv * 35.0) * 0.18;
      float horn_banding = abs(sin(v_bodyHeight * 25.0)) * 0.12;

      vec3 V = normalize(vec3(0.0, 1.0, 0.4));
      float view_angle = max(dot(normalize(v_worldNormal), V), 0.0);
      float horn_sheen = pow(1.0 - view_angle, 6.0) * 0.15;

      color *= 1.0 + wood_grain + horn_banding;
      color += vec3(horn_sheen);
    } else if (is_grip) {

      float leather_wrap = noise(uv * 20.0) * 0.14;
      float cord_binding = step(0.92, fract(v_worldPos.y * 30.0)) * 0.15;
      color *= vec3(0.55, 0.42, 0.32);
      color *= 1.0 + leather_wrap;
      color += vec3(cord_binding);
    } else if (is_string) {

      color = vec3(0.72, 0.68, 0.60);
      float fiber_twist = noise(uv * 80.0) * 0.10;
      color *= 1.0 + fiber_twist;
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

  float campaign_wear = (1.0 - v_bodyHeight * 0.6) * 0.15;
  float dust_accumulation = noise(v_worldPos.xz * 8.0) * campaign_wear * 0.12;

  float rain_streaks =
      smoothstep(0.85, 0.92,
                 noise(v_worldPos.xz * 2.5 + vec2(0.0, v_worldPos.y * 8.0))) *
      v_bodyHeight * 0.08;

  float mud_height = smoothstep(0.5, 0.2, v_bodyHeight);
  float mud_pattern =
      step(0.75, noise(v_worldPos.xz * 12.0 + v_worldPos.y * 3.0));
  float mud_splatter = mud_height * mud_pattern * 0.18;
  vec3 mud_color = vec3(0.22, 0.18, 0.14);

  color -= vec3(dust_accumulation);
  color -= vec3(rain_streaks * 0.5);
  color = mix(color, mud_color, mud_splatter);

  vec3 light_dir = normalize(vec3(1.0, 1.15, 1.0));
  float n_dot_l = dot(normal, light_dir);

  float wrap_amount =
      is_helmet ? 0.15 : (is_armor ? 0.22 : (is_greaves ? 0.15 : 0.38));
  float diff = max(n_dot_l * (1.0 - wrap_amount) + wrap_amount, 0.22);

  if (is_helmet || is_greaves) {
    diff = pow(diff, 0.90);
  }

  color *= diff;
  FragColor = vec4(color, alpha_in);
}
