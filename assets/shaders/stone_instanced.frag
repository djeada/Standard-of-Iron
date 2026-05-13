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
  float ambient = 0.24;
  vec3 sun_color = vec3(1.08, 0.92, 0.74);
  vec3 sky_color = vec3(0.72, 0.80, 1.00);
  float lit_t = clamp(diffuse * 1.4, 0.0, 1.0);
  vec3 light_tint = mix(sky_color * 0.50, sun_color, lit_t);
  float lighting = ambient + diffuse * 0.72;
  vec3 view_dir = normalize(vec3(0.0, 0.85, 0.53));
  vec3 half_vec = normalize(light_dir + view_dir);
  float spec_base = max(dot(normal, half_vec), 0.0);
  float stone_noise =
      0.5 + 0.5 * sin(v_local_pos.y * 18.0 + v_local_pos.x * 7.0 -
                      v_local_pos.z * 5.0);
  float fracture =
      smoothstep(0.82, 0.96,
                 abs(sin(v_local_pos.x * 16.0 + v_local_pos.z * 11.0 +
                         v_local_pos.y * 8.0)));
  float dust =
      smoothstep(0.10, 0.58, v_local_pos.y) * smoothstep(0.25, 0.85, normal.y);
  vec3 stone_color = v_color * mix(0.82, 1.12, stone_noise);
  stone_color += vec3(0.08, 0.06, 0.04) * dust * 0.35;
  stone_color -= vec3(0.10, 0.09, 0.08) * fracture * 0.20;
  float specular = pow(spec_base, 22.0) * 0.18;
  float rim = 1.0 - max(dot(normal, view_dir), 0.0);
  rim = pow(rim, 3.2) * 0.18;
  vec3 rim_color = sky_color * rim;
  float ao = clamp(normal.y * 0.60 + 0.72, 0.22, 1.0);
  vec3 color = stone_color * lighting * light_tint * ao;
  color += vec3(specular) * sun_color * ao;
  color += rim_color;
  frag_color = vec4(color, 1.0);
}
