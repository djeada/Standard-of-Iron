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
  float wheel_mask = mask_range(abs(v_local_pos.x), 0.66, 0.90, 0.05) *
                     mask_range(abs(v_local_pos.z), 0.24, 0.58, 0.07);
  float cargo_mask = mask_range(v_local_pos.y, 0.48, 1.02, 0.08) * (1.0 - wheel_mask);
  float wood_grain = 0.5 + 0.5 * sin(v_local_pos.z * 20.0 + v_local_pos.y * 11.0 +
                                     v_local_pos.x * 4.0);
  float rail_wear = smoothstep(0.40, 0.62, max(abs(v_local_pos.x), abs(v_local_pos.z)));
  vec3 wood_color = v_color * mix(0.86, 1.14, wood_grain);
  vec3 canvas_color = vec3(0.60, 0.54, 0.44);
  vec3 iron_color = vec3(0.36, 0.33, 0.29);
  vec3 material_color = wood_color;
  material_color =
      mix(material_color, mix(wood_color, canvas_color, 0.65), cargo_mask * 0.80);
  material_color = mix(material_color, iron_color, wheel_mask * 0.82);
  material_color = mix(material_color, material_color * 1.08, rail_wear * 0.12);

  float mud_height = 1.0 - smoothstep(0.00, 0.22, v_local_pos.y);
  float mud_noise = 0.5 + 0.5 * sin(v_world_pos.x * 3.4 + v_world_pos.z * 2.7) *
                              sin(v_local_pos.x * 5.0 + v_local_pos.z * 4.0);
  float mud = mud_height * mud_noise * 0.55;
  vec3 mud_color = vec3(0.28, 0.22, 0.16);
  material_color = mix(material_color, mud_color, mud * 0.48);
  float specular =
      mix(pow(spec_base, 26.0) * 0.10, pow(spec_base, 16.0) * 0.24, wheel_mask);
  float rim = 1.0 - max(dot(normal, view_dir), 0.0);
  rim = pow(rim, 3.5) * 0.16;
  vec3 rim_color = sky_color * rim;
  float ao = clamp(normal.y * 0.55 + 0.75, 0.22, 1.0);
  ao *= 1.0 - mask_range(v_local_pos.y, 0.00, 0.28, 0.08) * 0.12;
  vec3 color = material_color * lighting * light_tint * ao;
  color += vec3(specular) * sun_color * ao;
  color += rim_color;
  frag_color = vec4(color, 1.0);
}
