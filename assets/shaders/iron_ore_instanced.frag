#version 330 core

in vec3 v_world_pos;
in vec3 v_normal;
in vec3 v_color;
in vec3 v_local_pos;

uniform vec3 u_light_direction;

out vec4 frag_color;

void main() {
  vec3 normal = normalize(v_normal);
  vec3 light_dir = normalize(u_light_direction);
  float diffuse = max(dot(normal, light_dir), 0.0);
  float ambient = 0.22;
  vec3 sun_color = vec3(1.05, 0.88, 0.70);
  vec3 sky_color = vec3(0.62, 0.72, 0.95);
  float lit_t = clamp(diffuse * 1.3, 0.0, 1.0);
  vec3 light_tint = mix(sky_color * 0.45, sun_color, lit_t);
  float lighting = ambient + diffuse * 0.68;
  vec3 view_dir = normalize(vec3(0.0, 0.85, 0.53));
  vec3 half_vec = normalize(light_dir + view_dir);

  // Procedural rock surface noise
  float rock_noise =
      0.5 + 0.5 * sin(v_local_pos.y * 14.0 + v_local_pos.x * 9.0 - v_local_pos.z * 6.0);

  // Iron oxide vein pattern — layered bands with cross-cutting variation
  float vein = sin(v_local_pos.y * 22.0 + v_local_pos.x * 4.5 + v_local_pos.z * 3.2) *
               sin(v_local_pos.x * 18.0 - v_local_pos.z * 8.0);
  float vein_mask = smoothstep(0.35, 0.72, abs(vein));

  // Secondary cross-vein
  float vein2 = sin(v_local_pos.z * 20.0 + v_local_pos.y * 6.0) *
                sin(v_local_pos.x * 12.0 + v_local_pos.y * 3.0);
  float vein2_mask = smoothstep(0.42, 0.76, abs(vein2)) * 0.6;

  // Dark base rock — iron ore host rock is darker than plain stone
  vec3 rock_base = v_color * mix(0.65, 0.92, rock_noise);

  // Reddish-orange iron oxide / rust colour
  vec3 iron_color = vec3(0.72, 0.34, 0.13);
  float total_vein = clamp(vein_mask + vein2_mask, 0.0, 0.90);
  vec3 ore_color = mix(rock_base, iron_color, total_vein * 0.65);

  // Fracture darkening
  float fracture = smoothstep(
      0.78,
      0.95,
      abs(sin(v_local_pos.x * 15.0 + v_local_pos.z * 11.0 + v_local_pos.y * 7.0)));
  ore_color -= vec3(0.08, 0.07, 0.06) * fracture * 0.25;

  // Specular — brighter on ore-face, dimmer on plain rock
  float spec_base = max(dot(normal, half_vec), 0.0);
  float specular = pow(spec_base, 30.0) * 0.22 * (0.35 + total_vein * 0.85);

  float rim = 1.0 - max(dot(normal, view_dir), 0.0);
  rim = pow(rim, 3.2) * 0.12;
  vec3 rim_color = sky_color * rim;

  float ao = clamp(normal.y * 0.55 + 0.70, 0.20, 1.0);
  vec3 color = ore_color * lighting * light_tint * ao;
  color += vec3(specular) * mix(sun_color, iron_color, total_vein * 0.5) * ao;
  color += rim_color;

  frag_color = vec4(color, 1.0);
}
