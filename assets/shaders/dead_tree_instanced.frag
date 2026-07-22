#version 330 core

in vec3 v_world_pos;
in vec3 v_normal;
in vec3 v_color;
in vec3 v_local_pos;
in vec3 v_local_normal;

uniform vec3 u_light_direction;

out vec4 frag_color;

float hash12(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise21(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);

  float a = hash12(i);
  float b = hash12(i + vec2(1.0, 0.0));
  float c = hash12(i + vec2(0.0, 1.0));
  float d = hash12(i + vec2(1.0, 1.0));

  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main() {
  vec3 normal = normalize(v_normal);
  vec3 local_normal = normalize(v_local_normal);
  vec3 light_dir = normalize(u_light_direction);
  float ndotl = dot(normal, light_dir);
  float diffuse = max(ndotl, 0.0);
  float wrap = clamp((ndotl + 0.38) / 1.38, 0.0, 1.0);
  float ambient = 0.24;
  vec3 sun_color = vec3(0.94, 0.84, 0.70);
  vec3 sky_color = vec3(0.48, 0.56, 0.67);
  float lit_t = clamp(wrap * 1.15, 0.0, 1.0);
  vec3 light_tint = mix(sky_color * 0.58, sun_color, lit_t);
  float lighting = ambient + wrap * 0.46 + diffuse * 0.14;
  vec3 view_dir = normalize(vec3(0.0, 0.85, 0.53));
  vec3 half_vec = normalize(light_dir + view_dir);
  float spec_base = max(dot(normal, half_vec), 0.0);

  float bark_angle = atan(v_local_pos.z, v_local_pos.y - 0.16);
  float bark_axis = v_local_pos.x * 2.4;
  float bark_noise =
      noise21(vec2(bark_axis + bark_angle * 0.8, bark_angle * 2.0 + 4.0));
  float bark_furrow =
      pow(abs(sin(bark_angle * 5.0 + bark_axis * 0.8 + bark_noise * 1.6)), 0.32);
  float bark_streak = noise21(vec2(v_local_pos.x * 6.0, bark_angle * 3.0 + 9.0));
  float bark_variation =
      clamp(0.18 + bark_furrow * 0.56 + bark_streak * 0.22, 0.0, 1.0);
  float bark_crack =
      smoothstep(0.74, 0.92, bark_furrow) * smoothstep(0.52, 0.80, bark_streak);

  float abs_local_x = abs(v_local_pos.x);
  float end_mask = smoothstep(0.98, 1.08, abs_local_x);
  float cut_face_mask = end_mask * smoothstep(0.82, 0.96, abs(local_normal.x));

  float end_center_y = mix(0.18, 0.17, step(0.0, v_local_pos.x));
  float end_center_z = mix(-0.02, 0.02, step(0.0, v_local_pos.x));
  vec2 end_delta = vec2((v_local_pos.y - end_center_y) / 0.19,
                        (v_local_pos.z - end_center_z) / 0.20);
  float end_radius = length(end_delta);
  float end_noise =
      noise21(end_delta * 4.0 + vec2(abs_local_x * 3.0, bark_angle * 0.7 + 2.0));
  float end_rings = 0.5 + 0.5 * sin(end_radius * 18.0 - end_noise * 3.2);
  float end_angle = atan(end_delta.y, end_delta.x);
  float end_cracks =
      smoothstep(0.30, 0.92, end_radius) *
      (1.0 - smoothstep(0.02, 0.16, abs(sin(end_angle * 3.0 + end_noise * 2.6))));
  float end_rim = smoothstep(0.84, 1.02, end_radius);

  float top_face = clamp(local_normal.y, 0.0, 1.0);
  float moss_density = top_face * top_face;
  float moss_noise = noise21(v_local_pos.xz * 3.1 + v_world_pos.xz * 0.6);
  float near_ground_moss =
      1.0 - smoothstep(0.05, 0.48, max(v_local_pos.y, 0.0));
  float moss_t = clamp(moss_density * (0.25 + near_ground_moss * 0.75) * moss_noise *
                           (1.0 - cut_face_mask) * 0.48,
                       0.0,
                       1.0);

  vec3 bark_dark = v_color * vec3(0.54, 0.50, 0.46);
  vec3 bark_mid = v_color * vec3(0.82, 0.76, 0.68);
  vec3 bark_light = v_color * vec3(1.06, 0.98, 0.90);
  vec3 bark_color = mix(bark_dark, bark_mid, bark_variation);
  bark_color =
      mix(bark_color, bark_light, smoothstep(0.70, 0.96, bark_variation) * 0.30);
  bark_color *= 1.0 - bark_crack * 0.20;

  vec3 exposed_wood = mix(vec3(0.39, 0.32, 0.23),
                          vec3(0.57, 0.47, 0.33),
                          0.30 + end_rings * 0.35 + end_noise * 0.20);
  exposed_wood *= mix(1.0, 0.78, end_rim);
  exposed_wood *= 1.0 - end_cracks * 0.22;
  vec3 moss_color = vec3(0.20, 0.27, 0.17);

  vec3 material_color = mix(bark_color, exposed_wood, cut_face_mask);
  material_color =
      mix(material_color, exposed_wood, end_mask * (1.0 - cut_face_mask) * 0.18);
  material_color = mix(material_color, moss_color, moss_t);
  material_color *= mix(0.96, 1.03, noise21(v_world_pos.xz * 1.4 + vec2(7.0, 3.0)));

  float specular = pow(spec_base, 20.0) * 0.008;
  float rim = 1.0 - max(dot(normal, view_dir), 0.0);
  rim = pow(rim, 4.0) * 0.045;
  vec3 rim_color = sky_color * rim;
  float ao = clamp(local_normal.y * 0.34 + 0.82, 0.56, 1.0);
  float underside = 1.0 - smoothstep(-0.88, -0.32, local_normal.y);
  float near_ground = 1.0 - smoothstep(-0.10, 0.05, v_local_pos.y);
  float contact_shadow = underside * near_ground * (1.0 - cut_face_mask) * 0.24;

  ao *= 1.0 - contact_shadow;

  vec3 color = material_color * lighting * light_tint * ao;
  color += vec3(specular) * sun_color * ao;
  color += rim_color;
  frag_color = vec4(color, 1.0);
}
