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

  float ambient = 0.20;
  vec3 sun_color = vec3(1.06, 0.94, 0.78);
  vec3 sky_color = vec3(0.68, 0.78, 1.00);
  float lit_t = clamp(diffuse * 1.3, 0.0, 1.0);
  vec3 light_tint = mix(sky_color * 0.45, sun_color, lit_t);

  float lighting = ambient + diffuse * 0.72;

  vec3 view_dir = normalize(vec3(0.0, 0.85, 0.53));
  vec3 half_vec = normalize(light_dir + view_dir);
  float spec_base = max(dot(normal, half_vec), 0.0);

  float warp = 0.5 + 0.5 * sin(v_local_pos.y * 26.0 + v_local_pos.x * 3.0);
  float weft = 0.5 + 0.5 * sin(v_local_pos.z * 26.0 + v_local_pos.x * 3.0);
  float weave = mix(warp, weft, 0.5);

  float seam =
      smoothstep(0.38, 0.48, abs(fract(v_local_pos.z * 1.8) - 0.5)) * 0.12;

  float top_bleach = smoothstep(0.52, 1.10, v_local_pos.y) * 0.18;
  float base_dirt = (1.0 - smoothstep(0.00, 0.24, v_local_pos.y)) * 0.28;

  float stain = (0.5 + 0.5 * sin(v_world_pos.x * 1.6 + v_world_pos.z * 2.1) *
                           sin(v_local_pos.y * 1.4 + v_local_pos.z * 1.8)) *
                0.10;

  vec3 canvas_color = v_color;
  canvas_color *= mix(0.90, 1.10, weave * 0.5 + 0.5);
  canvas_color -= seam;
  canvas_color += top_bleach;
  canvas_color -= vec3(0.14, 0.10, 0.06) * base_dirt;
  canvas_color -= stain;

  float specular = pow(spec_base, 52.0) * 0.05;

  float rim = 1.0 - max(dot(normal, view_dir), 0.0);
  rim = pow(rim, 3.5) * 0.12;
  vec3 rim_color = sky_color * rim;

  float ao = clamp(normal.y * 0.55 + 0.75, 0.22, 1.0);

  ao *= 1.0 - (1.0 - smoothstep(0.00, 0.18, v_local_pos.y)) * 0.14;

  vec3 color = canvas_color * lighting * light_tint * ao;
  color += vec3(specular) * sun_color * ao;
  color += rim_color;

  frag_color = vec4(color, 1.0);
}
