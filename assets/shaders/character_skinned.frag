#version 330 core

in vec3 v_normal_ws;
in vec2 v_tex;
in vec3 v_pos_ws;
in vec3 v_pos_local;
flat in int v_color_role;
flat in vec4 v_wear_params;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_use_texture;
uniform float u_alpha;
uniform int u_material_id;
uniform vec3 u_role_colors[32];
uniform int u_role_color_count;

out vec4 frag_color;

float hash13(vec3 p) {
  return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

vec3 apply_wear(vec3 base, int material_id, int color_role, vec3 pos_local, vec4 wear) {
  float wear_amount = clamp(wear.x * 1.35, 0.0, 1.0);
  float grime_amount = clamp(wear.y * 1.35, 0.0, 1.0);
  float blood_amount = clamp(wear.z * 1.75, 0.0, 1.0);
  float seed = wear.w + float(material_id) * 0.173;

  vec3 abs_pos = abs(pos_local);
  vec3 mask_p = abs_pos * (2.8 + float(material_id & 3)) + vec3(seed * 13.0);
  float macro = hash13(floor(mask_p * 2.0) + 3.0);
  float blotch = hash13(floor(mask_p * 3.0));
  float micro = hash13(floor(mask_p * 9.0) + 17.0);
  float streak = 0.5 + 0.5 * sin(mask_p.y * 8.0 + seed * 19.0 + mask_p.z * 6.0);
  float edge_mask =
      smoothstep(0.18, 0.95, max(abs_pos.x, max(abs_pos.y * 0.75, abs_pos.z)));
  float grime_mask =
      smoothstep(0.18, 0.88, macro * 0.45 + blotch * 0.30 + streak * 0.25);
  float wear_mask =
      max(edge_mask * 0.75, smoothstep(0.34, 0.86, blotch * 0.55 + micro * 0.45));
  float fade_mask = smoothstep(0.28, 0.82, hash13(floor(mask_p.yzx * 4.0) + 29.0));
  float blood_mask = smoothstep(0.66, 0.94, hash13(floor(mask_p.yzx * 5.0) + 23.0));
  float shade_variation = 0.78 + 0.42 * hash13(floor(mask_p.zxy * 3.0) + 41.0);

  float max_component = max(base.r, max(base.g, base.b));
  float min_component = min(base.r, min(base.g, base.b));
  float saturation = max_component - min_component;
  float brightness = (base.r + base.g + base.b) / 3.0;
  float metal_like = max(float(material_id == 2 || material_id == 6),
                         smoothstep(0.18, 0.58, max_component) *
                             (1.0 - smoothstep(0.08, 0.30, saturation)));
  float leather_like =
      max(float(material_id == 1 || material_id == 3 || material_id == 4),
          (1.0 - metal_like) * smoothstep(0.08, 0.32, saturation) *
              smoothstep(0.18, 0.72, base.r));
  float cloth_like = clamp(1.0 - metal_like * 0.8, 0.0, 1.0);
  if (color_role == 2) {
    wear_amount *= 0.45;
    grime_amount *= 0.18;
    blood_amount *= 0.08;
  }

  vec3 worn = base;
  vec3 grayscale = vec3(brightness);
  worn *= mix(1.0, shade_variation, 0.20 * cloth_like + 0.12 * leather_like);
  worn = mix(worn,
             worn * vec3(0.62, 0.58, 0.52),
             wear_amount * fade_mask * (0.40 * cloth_like + 0.14 * leather_like));
  worn = mix(worn,
             grayscale * vec3(0.96, 0.92, 0.84),
             wear_amount * wear_mask * (0.14 + 0.32 * metal_like));
  worn *= 1.0 -
          grime_amount * grime_mask * (0.24 + 0.18 * cloth_like + 0.14 * leather_like);
  worn = mix(worn,
             worn * vec3(0.55, 0.72, 0.52),
             wear_amount * grime_mask * metal_like * 0.34);
  worn = mix(worn,
             worn * vec3(0.72, 0.56, 0.44),
             grime_amount * grime_mask * leather_like * 0.26);
  worn = mix(worn,
             min(worn * 1.22, vec3(1.0)),
             wear_amount * micro * edge_mask * metal_like * 0.14);
  if (color_role == 2) {
    float bruise_mask = smoothstep(0.74, 0.96, hash13(floor(mask_p * 6.0) + 59.0));
    worn = mix(worn, worn * vec3(0.82, 0.68, 0.68), wear_amount * bruise_mask * 0.22);
  }
  worn =
      mix(worn,
          vec3(0.42, 0.09, 0.07),
          blood_amount * blood_mask * (0.18 + 0.60 * cloth_like + 0.24 * leather_like));
  return clamp(worn, 0.0, 1.0);
}

void main() {
  vec3 base = u_color;
  if (v_color_role > 0 && v_color_role <= u_role_color_count) {
    base = u_role_colors[v_color_role - 1];
  }
  if (u_use_texture) {
    base *= texture(u_texture, v_tex).rgb;
  }
  base = apply_wear(base, u_material_id, v_color_role, v_pos_local, v_wear_params);

  vec3 normal = normalize(v_normal_ws);
  vec3 light_dir = normalize(vec3(0.65, 0.50, 0.40));

  vec3 sun_color = vec3(1.08, 0.92, 0.74);
  vec3 sky_color = vec3(0.72, 0.80, 1.00);

  float wrap = 0.28;
  float ndl = dot(normal, light_dir);
  float diff_raw = ndl * (1.0 - wrap) + wrap;
  float diff = max(diff_raw, 0.15);

  float lit_t = clamp((diff_raw + 1.0) / 2.5, 0.0, 1.0);
  vec3 light_tint = mix(sky_color * 0.34, sun_color, lit_t);

  vec3 color = clamp(base * diff * light_tint, 0.0, 1.0);
  frag_color = vec4(color, u_alpha);
}
