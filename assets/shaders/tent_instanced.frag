#version 330 core

in vec3 v_world_pos;
in vec3 v_normal;
in vec3 v_color;

uniform vec3 u_light_direction;

out vec4 frag_color;

void main() {
  vec3 normal = normalize(v_normal);
  vec3 light_dir = normalize(u_light_direction);

  float diffuse = max(dot(normal, light_dir), 0.0);

  float ambient = 0.28;
  vec3 sun_color = vec3(1.06, 0.94, 0.78);
  vec3 sky_color = vec3(0.68, 0.78, 1.00);
  float lit_t = clamp(diffuse * 1.3, 0.0, 1.0);
  vec3 light_tint = mix(sky_color * 0.45, sun_color, lit_t);

  float lighting = ambient + diffuse * 0.65;

  // Specular sheen — fabric has soft, low specular
  // Isometric game with fixed elevated camera angle.
  vec3 view_dir = normalize(vec3(0.0, 0.85, 0.53));
  vec3 half_vec = normalize(light_dir + view_dir);
  float spec_base = max(dot(normal, half_vec), 0.0);
  float specular = pow(spec_base, 28.0) * 0.14;

  // Rim lighting adds sky-tinted silhouette edge
  float rim = 1.0 - max(dot(normal, view_dir), 0.0);
  rim = pow(rim, 3.5) * 0.16;
  vec3 rim_color = sky_color * rim;

  // Normal-based AO: downward-facing faces get softer shadow
  float ao = clamp(normal.y * 0.55 + 0.75, 0.28, 1.0);

  vec3 color = v_color * lighting * light_tint * ao;
  color += vec3(specular) * sun_color * ao;
  color += rim_color;

  frag_color = vec4(color, 1.0);
}
