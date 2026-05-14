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
  float stone_noise =
      0.5 + 0.5 * sin(v_local_pos.y * 12.0 + v_local_pos.x * 8.0 + v_local_pos.z * 5.0);
  float lichen = mask_range(v_local_pos.y, 0.12, 0.72, 0.12) *
                 (0.5 + 0.5 * sin(v_world_pos.x * 2.8 + v_world_pos.z * 3.2));
  float dust = smoothstep(0.72, 1.52, v_local_pos.y);
  float fracture = smoothstep(
      0.88,
      0.98,
      abs(sin(v_local_pos.x * 18.0 - v_local_pos.z * 13.0 + v_local_pos.y * 9.0)));
  vec3 stone_color = v_color * mix(0.82, 1.14, stone_noise);
  stone_color = mix(stone_color, stone_color * vec3(0.82, 0.84, 0.80), fracture * 0.35);
  stone_color += vec3(0.06, 0.07, 0.04) * lichen * 0.45;
  stone_color += vec3(0.10, 0.09, 0.07) * dust * 0.20;

  float mortar_h = 1.0 - smoothstep(0.04, 0.16, fract(v_local_pos.y * 2.8));
  float mortar_v =
      1.0 - smoothstep(0.04, 0.16, fract(v_local_pos.x * 1.9 + v_local_pos.z * 1.4));
  stone_color *= 1.0 - max(mortar_h, mortar_v) * 0.20;

  float base_stain = (1.0 - smoothstep(0.00, 0.34, v_local_pos.y)) * 0.32;
  stone_color -= vec3(0.08, 0.07, 0.05) * base_stain;
  float specular = pow(spec_base, 30.0) * 0.10;
  float rim = 1.0 - max(dot(normal, view_dir), 0.0);
  rim = pow(rim, 3.5) * 0.14;
  vec3 rim_color = sky_color * rim;
  float ao = clamp(normal.y * 0.55 + 0.75, 0.22, 1.0);
  vec3 color = stone_color * lighting * light_tint * ao;
  color += vec3(specular) * sun_color * ao;
  color += rim_color;
  frag_color = vec4(color, 1.0);
}
