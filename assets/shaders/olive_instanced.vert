#version 330 core

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_tex_coord;
layout(location = 2) in vec3 a_normal;
layout(location = 3) in vec4 a_pos_scale;
layout(location = 4) in vec4 a_color_sway;
layout(location = 5) in vec4 a_rotation;

layout(std140) uniform FrameData {
  mat4 u_view_proj;
};
uniform float u_time;
uniform float u_wind_strength;
uniform float u_wind_speed;

out vec3 v_world_pos;
out vec3 v_normal;
out vec3 v_color;
out vec2 v_tex_coord;
out float v_foliage_mask;
out float v_leaf_seed;
out float v_bark_seed;
out float v_branch_id;
out vec2 v_local_pos_xz;
out vec3 v_local_pos;

void main() {
  const float TWO_PI = 6.2831853;

  float scale = a_pos_scale.w;
  vec3 world_pos = a_pos_scale.xyz;
  float sway_phase = a_color_sway.a;
  float rotation = a_rotation.x;
  float silhouette_seed = a_rotation.y;
  float leaf_seed = a_rotation.z;
  float bark_seed = a_rotation.w;

  vec3 model_pos = a_pos;

  float trunk_mask = 1.0 - smoothstep(0.12, 0.20, a_tex_coord.y);
  float foliage_mask = smoothstep(0.45, 0.55, a_tex_coord.y);

  float angle = a_tex_coord.x * TWO_PI;
  float branch_id = floor(angle / TWO_PI * 4.0 + silhouette_seed * 4.0);

  if (trunk_mask > 0.0) {
    float twist = sin(a_tex_coord.y * 22.0 + bark_seed * TWO_PI) * 0.034;
    float writhe = sin(a_tex_coord.y * 9.0 - bark_seed * TWO_PI * 1.7) * 0.022;
    model_pos.x += (twist + writhe * 0.6) * trunk_mask;
    model_pos.z += (twist * 0.7 - writhe) * trunk_mask;
    float ang_t = a_tex_coord.x * TWO_PI;
    float bulge = 1.0 + sin(ang_t * 3.0 + a_tex_coord.y * 14.0 + bark_seed * TWO_PI) *
                            0.10 * trunk_mask;
    model_pos.xz *= bulge;
  }

  float height_norm = clamp(a_pos.y / 1.10, 0.0, 1.0);
  float lean_angle = (silhouette_seed - 0.5) * 0.22;
  float lean_yaw = bark_seed * TWO_PI;
  model_pos.x += cos(lean_yaw) * lean_angle * height_norm * height_norm;
  model_pos.z += sin(lean_yaw) * lean_angle * height_norm * height_norm;

  if (foliage_mask > 0.1) {
    float ang = atan(model_pos.z, model_pos.x);
    float lump_base = sin(ang * 2.6 + silhouette_seed * TWO_PI) * 0.085;
    float lump_fine = sin(ang * 5.4 + leaf_seed * TWO_PI * 1.9) * 0.042;
    float lump_ragged =
        sin(ang * 5.0 + a_pos.y * 16.0 + leaf_seed * TWO_PI * 3.1) * 0.040;
    float lump_tuft =
        sin(ang * 3.0 - a_pos.y * 23.0 + silhouette_seed * TWO_PI * 2.3) * 0.030;
    float lump_mag = (lump_base + lump_fine + lump_ragged + lump_tuft) * foliage_mask;
    model_pos.xz *= (1.0 + lump_mag);
    model_pos.y += (lump_ragged + lump_tuft) * 0.55 * foliage_mask;

    float canopy_height = clamp((a_pos.y - 0.34) / 0.76, 0.0, 1.0);
    float upper_taper = mix(0.98, 0.72, canopy_height);
    float lower_fill = mix(1.05, 0.94, canopy_height);
    float canopy_profile =
        mix(lower_fill, upper_taper, smoothstep(0.10, 0.96, canopy_height));
    model_pos.xz *= canopy_profile;

    float radial = length(model_pos.xz);
    model_pos.y += canopy_height * (0.05 + leaf_seed * 0.08) -
                   radial * radial * (0.03 + 0.02 * canopy_height);

    float stretch = mix(1.02, 1.30, leaf_seed);
    model_pos.y *= mix(1.0, stretch, foliage_mask);

    float spread = mix(0.96, 1.14, fract(silhouette_seed * 7.31 + leaf_seed * 3.17));
    model_pos.xz *= mix(1.0, spread, foliage_mask);
  }

  vec3 local_pos = model_pos * scale;

  float height_factor = clamp(a_pos.y / 1.10, 0.0, 1.0);
  float bend = height_factor * height_factor;
  float wind_time = u_time * u_wind_speed * 0.4;
  float gust = 0.60 + 0.40 * sin(wind_time * 0.23 + sway_phase * 0.41);
  float sway = sin(wind_time + sway_phase) * u_wind_strength * gust * bend;
  float sway2 = sin(wind_time * 1.7 + sway_phase * 2.3) * u_wind_strength * 0.38 * bend;

  float sway_amount = mix(0.05, 0.20, foliage_mask) * scale;
  vec2 wind_dir = normalize(vec2(0.78, 0.62));
  vec2 cross_dir = vec2(-wind_dir.y, wind_dir.x);
  vec2 wind_offset = (wind_dir * sway + cross_dir * sway2) * sway_amount;
  float canopy_edge = smoothstep(0.12, 0.40, length(model_pos.xz));
  float leaf_flutter =
      sin(wind_time * 3.15 + branch_id * 1.73 + leaf_seed * TWO_PI + angle * 1.4) *
      sin(wind_time * 1.21 + sway_phase * 0.83 + silhouette_seed * TWO_PI);
  float flutter_flex = foliage_mask * mix(0.42, 1.0, canopy_edge);
  wind_offset +=
      cross_dir * leaf_flutter * flutter_flex * gust * u_wind_strength * 0.022 * scale;
  local_pos.y += leaf_flutter * flutter_flex * u_wind_strength * 0.006 * scale;
  local_pos.y -= abs(sway) * 0.012 * foliage_mask * scale;

  float cos_r = cos(rotation);
  float sin_r = sin(rotation);
  mat2 rot = mat2(cos_r, -sin_r, sin_r, cos_r);

  vec2 rotated_xz = rot * local_pos.xz + wind_offset;
  local_pos = vec3(rotated_xz.x, local_pos.y, rotated_xz.y);

  vec2 rotated_normal_xz = rot * a_normal.xz;
  vec3 final_normal =
      normalize(vec3(rotated_normal_xz.x, a_normal.y, rotated_normal_xz.y));

  v_world_pos = local_pos + world_pos;
  v_normal = final_normal;
  v_color = a_color_sway.rgb;
  v_tex_coord = a_tex_coord;
  v_foliage_mask = foliage_mask;
  v_leaf_seed = leaf_seed;
  v_bark_seed = bark_seed;
  v_branch_id = branch_id;
  v_local_pos_xz = model_pos.xz;
  v_local_pos = model_pos;

  gl_Position = u_view_proj * vec4(v_world_pos, 1.0);
}
