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
  float ndotl = dot(normal, light_dir);
  float diffuse = max(ndotl, 0.0);
  float wrap = clamp((ndotl + 0.38) / 1.38, 0.0, 1.0);
  float ambient = 0.32;
  vec3 sun_color = vec3(1.06, 0.94, 0.78);
  vec3 sky_color = vec3(0.68, 0.78, 1.00);
  float lit_t = clamp(wrap * 1.15, 0.0, 1.0);
  vec3 light_tint = mix(sky_color * 0.45, sun_color, lit_t);
  float lighting = ambient + wrap * 0.56 + diffuse * 0.18;
  vec3 view_dir = normalize(vec3(0.0, 0.85, 0.53));
  vec3 half_vec = normalize(light_dir + view_dir);
  float spec_base = max(dot(normal, half_vec), 0.0);

  float bark_grain = 0.5 + 0.5 * sin(v_local_pos.x * 22.0 +
                                     v_local_pos.y * 8.0 - v_local_pos.z * 5.0);

  float end_wood = smoothstep(0.58, 0.76, abs(v_local_pos.x));

  float splinter = smoothstep(0.66, 0.80, -v_local_pos.x) *
                   (1.0 - smoothstep(0.06, 0.18, abs(v_local_pos.y - 0.10)));

  float top_face = clamp(normal.y, 0.0, 1.0);
  float moss_density = top_face * top_face * top_face;
  float moss_noise = 0.5 + 0.5 *
                               sin(v_local_pos.x * 4.3 + v_world_pos.z * 3.1) *
                               sin(v_local_pos.z * 5.7 + v_world_pos.x * 2.8);
  float moss_t = clamp(moss_density * moss_noise * 1.2, 0.0, 1.0);

  float weathering = 0.5 + 0.5 * sin(v_world_pos.x * 2.2 + v_world_pos.z * 2.8);

  vec3 bark_color = v_color * mix(0.78, 1.16, bark_grain);
  vec3 exposed_wood = vec3(0.70, 0.56, 0.36);
  vec3 moss_color = vec3(0.24, 0.38, 0.16);

  float wood_t = clamp(end_wood + splinter * 0.55, 0.0, 1.0);
  vec3 material_color = mix(bark_color, exposed_wood, wood_t);
  material_color = mix(material_color, moss_color, moss_t * 0.55);
  material_color *= mix(0.94, 1.06, weathering);

  float specular = pow(spec_base, 48.0) * 0.03;
  float rim = 1.0 - max(dot(normal, view_dir), 0.0);
  rim = pow(rim, 4.0) * 0.12;
  vec3 rim_color = sky_color * rim;
  float ao = clamp(normal.y * 0.40 + 0.82, 0.50, 1.0);

  ao *= 1.0 - clamp(-normal.y * 0.22, 0.0, 0.22);

  vec3 color = material_color * lighting * light_tint * ao;
  color += vec3(specular) * sun_color * ao;
  color += rim_color;
  frag_color = vec4(color, 1.0);
}
