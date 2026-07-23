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

float band(float value, float center, float half_width, float feather) {
  return 1.0 - smoothstep(half_width, half_width + feather, abs(value - center));
}

void main() {
  vec3 N = normalize(v_normal);
  vec3 L = normalize(u_light_direction);
  vec3 V = normalize(vec3(0.0, 0.86, 0.52));
  vec3 H = normalize(L + V);

  float wheel_x = smoothstep(0.67, 0.72, abs(v_local_pos.x)) *
                  (1.0 - smoothstep(0.86, 0.90, abs(v_local_pos.x)));
  float rear = step(0.0, v_local_pos.z);
  vec2 wheel_center = mix(vec2(0.24, -0.48), vec2(0.30, 0.42), rear);
  float wheel_radius = mix(0.22, 0.28, rear);
  float radial = length(v_local_pos.yz - wheel_center);
  float wheel_mask =
      wheel_x * (1.0 - smoothstep(wheel_radius + 0.01, wheel_radius + 0.045, radial));
  float tyre_mask =
      wheel_mask * smoothstep(wheel_radius - 0.055, wheel_radius - 0.018, radial);
  float hub_mask = wheel_mask * (1.0 - smoothstep(0.055, 0.105, radial));

  float cargo_core = (1.0 - smoothstep(0.43, 0.50, abs(v_local_pos.x))) *
                     smoothstep(0.49, 0.57, v_local_pos.y) *
                     (1.0 - smoothstep(1.01, 1.07, v_local_pos.y));
  float barrel_mask = cargo_core * (1.0 - smoothstep(0.32, 0.48, abs(v_local_pos.z)));
  float crate_mask = cargo_core * smoothstep(0.12, 0.25, abs(v_local_pos.z));
  float canvas_mask = cargo_core * smoothstep(0.80, 0.88, v_local_pos.y);

  float longitudinal_grain =
      0.5 + 0.5 * sin(v_local_pos.z * 31.0 + v_local_pos.y * 5.0);
  float cross_grain = hash12(floor(v_local_pos.zy * 18.0) + v_world_pos.xz * 0.15);
  vec3 dark_wood = v_color * vec3(0.62, 0.58, 0.52);
  vec3 fresh_wood = v_color * vec3(1.28, 1.12, 0.88);
  vec3 wood =
      mix(dark_wood, fresh_wood, longitudinal_grain * 0.62 + cross_grain * 0.38);

  float plank_line = max(band(fract((v_local_pos.z + 0.72) * 4.2), 0.5, 0.025, 0.018),
                         band(fract((v_local_pos.y - 0.30) * 6.0), 0.5, 0.020, 0.016));
  wood *= mix(1.0, 0.62, plank_line * 0.46);

  vec3 wheel_wood =
      mix(vec3(0.23, 0.115, 0.045), vec3(0.52, 0.29, 0.10), longitudinal_grain);
  vec3 iron =
      mix(vec3(0.12, 0.14, 0.16), vec3(0.38, 0.36, 0.32), hash12(v_world_pos.xz * 8.0));
  vec3 barrel = mix(vec3(0.35, 0.18, 0.07), vec3(0.67, 0.39, 0.13), longitudinal_grain);
  float barrel_band = band(fract((v_local_pos.y - 0.50) * 5.2), 0.5, 0.055, 0.025);
  barrel = mix(barrel, vec3(0.16, 0.17, 0.18), barrel_band * 0.72);
  vec3 crate = mix(vec3(0.40, 0.24, 0.09), vec3(0.70, 0.47, 0.19), cross_grain);
  vec3 canvas = mix(vec3(0.36, 0.39, 0.23),
                    vec3(0.60, 0.53, 0.31),
                    hash12(floor(v_local_pos.xz * 12.0)));

  vec3 albedo = wood;
  albedo = mix(albedo, wheel_wood, wheel_mask);
  albedo = mix(albedo, iron, max(tyre_mask, hub_mask));
  albedo = mix(albedo, barrel, barrel_mask);
  albedo = mix(albedo, crate, crate_mask * (1.0 - barrel_mask));
  albedo = mix(albedo, canvas, canvas_mask);

  float mud_height = 1.0 - smoothstep(0.04, 0.30, v_local_pos.y);
  float mud_noise = hash12(floor(v_world_pos.xz * 5.0));
  float mud = mud_height * smoothstep(0.34, 0.78, mud_noise);
  albedo = mix(albedo, vec3(0.24, 0.17, 0.105), mud * 0.58);

  float ndotl = max(dot(N, L), 0.0);
  float hemi = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
  vec3 sky = vec3(0.49, 0.57, 0.67);
  vec3 sun = vec3(0.99, 0.86, 0.67);
  vec3 illumination = sky * (0.17 + hemi * 0.14) + sun * ndotl * 0.74;
  float cavity = mix(0.56, 1.0, hemi) * mix(1.0, 0.78, cargo_core);
  float metal_mask = max(tyre_mask, hub_mask);
  float specular = mix(pow(max(dot(N, H), 0.0), 24.0) * 0.035,
                       pow(max(dot(N, H), 0.0), 54.0) * 0.32,
                       metal_mask);
  float rim = pow(1.0 - max(dot(N, V), 0.0), 4.0) * 0.045;

  vec3 color = albedo * illumination * cavity;
  color += sun * specular;
  color += sky * rim;
  frag_color = vec4(color, 1.0);
}
