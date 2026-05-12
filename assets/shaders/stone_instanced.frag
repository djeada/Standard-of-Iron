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

  float ambient = 0.24;
  float lighting = ambient + diffuse * 0.72;

  vec3 sun_color = vec3(1.08, 0.92, 0.74);
  vec3 sky_color = vec3(0.72, 0.80, 1.00);
  float lit_t = clamp(diffuse * 1.4, 0.0, 1.0);
  vec3 light_tint = mix(sky_color * 0.50, sun_color, lit_t);

  // Specular highlight using approximate overhead camera direction
  vec3 view_dir = normalize(vec3(0.0, 0.85, 0.53));
  vec3 half_vec = normalize(light_dir + view_dir);
  float spec_base = max(dot(normal, half_vec), 0.0);
  float specular = pow(spec_base, 18.0) * 0.28;

  // Rim lighting to improve silhouette visibility
  float rim = 1.0 - max(dot(normal, view_dir), 0.0);
  rim = pow(rim, 3.2) * 0.18;
  vec3 rim_color = sky_color * rim;

  // Height-based ambient occlusion using normal orientation
  // Downward-facing faces (bottom) get more shadow, top faces get more light
  float ao = clamp(normal.y * 0.60 + 0.72, 0.22, 1.0);

  vec3 color = v_color * lighting * light_tint * ao;
  color += vec3(specular) * sun_color * ao;
  color += rim_color;

  frag_color = vec4(color, 1.0);
}
