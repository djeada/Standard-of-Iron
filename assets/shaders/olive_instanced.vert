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
    float twist = sin(a_tex_coord.y * 20.0 + bark_seed * TWO_PI) * 0.02;
    model_pos.x += twist * trunk_mask;
    model_pos.z += twist * 0.7 * trunk_mask;
  }

  float height_norm = clamp(a_pos.y / 0.55, 0.0, 1.0);
  float lean_angle = (silhouette_seed - 0.5) * 0.22;
  float lean_yaw = bark_seed * TWO_PI;
  model_pos.x += cos(lean_yaw) * lean_angle * height_norm * height_norm;
  model_pos.z += sin(lean_yaw) * lean_angle * height_norm * height_norm;

  if (foliage_mask > 0.1) {
    float ang = atan(model_pos.z, model_pos.x);
    float lump_base = sin(ang * 3.0 + silhouette_seed * TWO_PI) * 0.10;
    float lump_fine = sin(ang * 5.0 + leaf_seed * TWO_PI * 1.7) * 0.04;
    float lump_mag = (lump_base + lump_fine) * foliage_mask;
    model_pos.xz *= (1.0 + lump_mag);

    float stretch = mix(0.88, 1.14, leaf_seed);
    model_pos.y *= mix(1.0, stretch, foliage_mask);
  }

  vec3 local_pos = model_pos * scale;

  float height_factor = clamp(a_pos.y * 2.0, 0.0, 1.0);
  float wind_time = u_time * u_wind_speed * 0.4;
  float sway = sin(wind_time + sway_phase) * u_wind_strength * 0.3;
  float sway2 = sin(wind_time * 1.7 + sway_phase * 2.3) * u_wind_strength * 0.15;

  float sway_amount = mix(0.02, 0.12, foliage_mask) * height_factor;
  local_pos.x += sway * sway_amount;
  local_pos.z += sway2 * sway_amount * 0.6;

  float cos_r = cos(rotation);
  float sin_r = sin(rotation);
  mat2 rot = mat2(cos_r, -sin_r, sin_r, cos_r);

  vec2 rotated_xz = rot * local_pos.xz;
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

  gl_Position = u_view_proj * vec4(v_world_pos, 1.0);
}
