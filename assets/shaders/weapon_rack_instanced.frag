#version 330 core

in vec3 v_world_pos;
in vec3 v_normal;
in vec3 v_color;
in vec3 v_local_pos;

uniform vec3 u_light_direction;

out vec4 frag_color;

float mask_range(float value, float lo, float hi, float feather) {
  return smoothstep(lo - feather, lo + feather, value) *
         (1.0 - smoothstep(hi - feather, hi + feather, value));
}

void main() {
  vec3 normal = normalize(v_normal);
  vec3 light_dir = normalize(u_light_direction);
  float diffuse = max(dot(normal, light_dir), 0.0);
  float ambient = 0.20;
  vec3 sun_color = vec3(1.06, 0.94, 0.78);
  vec3 sky_color = vec3(0.68, 0.78, 1.00);
  float lit_t = clamp(diffuse * 1.3, 0.0, 1.0);
  vec3 light_tint = mix(sky_color * 0.45, sun_color, lit_t);
  float lighting = ambient + diffuse * 0.72;
  vec3 view_dir = normalize(vec3(0.0, 0.85, 0.53));
  vec3 half_vec = normalize(light_dir + view_dir);
  float spec_base = max(dot(normal, half_vec), 0.0);
  float metal_mask = 0.0;
  metal_mask = max(metal_mask, mask_range(v_local_pos.x, -0.50, -0.28, 0.05) *
                                   mask_range(v_local_pos.y, 0.06, 1.46, 0.08));
  metal_mask = max(metal_mask, mask_range(v_local_pos.x, -0.10, 0.06, 0.05) *
                                   mask_range(v_local_pos.y, 0.10, 1.24, 0.08));
  metal_mask = max(metal_mask, mask_range(v_local_pos.x, 0.28, 0.48, 0.05) *
                                   mask_range(v_local_pos.y, 0.06, 1.44, 0.08));
  metal_mask = max(metal_mask, mask_range(v_local_pos.x, 0.04, 0.44, 0.05) *
                                   mask_range(v_local_pos.y, 0.22, 0.74, 0.06));
  float shield_mask =
      mask_range(v_local_pos.x, -0.72, -0.54, 0.04) *
      (1.0 - smoothstep(0.20, 0.28,
                        length(vec2(v_local_pos.y - 0.48, v_local_pos.z))));
  float wood_grain =
      0.5 + 0.5 * sin(v_local_pos.y * 13.0 + v_local_pos.x * 7.0);
  vec3 wood_color = v_color * mix(0.84, 1.10, wood_grain);
  vec3 steel_color = vec3(0.42, 0.42, 0.44);
  vec3 leather_color = vec3(0.38, 0.19, 0.10);
  vec3 material_color = wood_color;
  material_color = mix(material_color, steel_color, metal_mask * 0.82);
  material_color = mix(material_color, leather_color, shield_mask * 0.86);

  float rust_streak =
      smoothstep(0.78, 0.94,
                 abs(sin(v_local_pos.y * 6.2 + v_local_pos.x * 2.4))) *
      metal_mask;
  vec3 rust_color = vec3(0.50, 0.22, 0.08);
  material_color = mix(material_color, rust_color, rust_streak * 0.38);

  float wood_nick = smoothstep(
      0.72, 0.84, abs(sin(v_local_pos.y * 10.0 + v_local_pos.z * 3.2)));
  material_color *=
      1.0 - wood_nick * (1.0 - metal_mask) * (1.0 - shield_mask) * 0.16;
  float specular =
      mix(pow(spec_base, 26.0) * 0.10, pow(spec_base, 14.0) * 0.30, metal_mask);
  float rim = 1.0 - max(dot(normal, view_dir), 0.0);
  rim = pow(rim, 3.0) * 0.18;
  vec3 rim_color = sky_color * rim;
  float ao = clamp(normal.y * 0.55 + 0.75, 0.22, 1.0);
  vec3 color = material_color * lighting * light_tint * ao;
  color += vec3(specular) * sun_color * ao;
  color += rim_color;
  frag_color = vec4(color, 1.0);
}
