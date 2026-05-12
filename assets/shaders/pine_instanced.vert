#version 330 core

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_tex_coord;
layout(location = 2) in vec3 a_normal;
layout(location = 3) in vec4 a_pos_scale;
layout(location = 4) in vec4 a_color_sway;
layout(location = 5) in vec4 a_rotation;

layout(std140) uniform FrameData { mat4 u_view_proj; };
uniform float u_time;
uniform float u_wind_strength;
uniform float u_wind_speed;

out vec3 v_world_pos;
out vec3 v_normal;
out vec3 v_color;
out vec2 v_tex_coord;
out float v_foliage_mask;
out float v_needle_seed;
out float v_bark_seed;

void main() {
  const float TWO_PI = 6.2831853;

  float scale = a_pos_scale.w;
  vec3 world_pos = a_pos_scale.xyz;
  float sway_phase = a_color_sway.a;
  float rotation = a_rotation.x;
  float silhouette_seed = a_rotation.y;
  float needle_seed = a_rotation.z;
  float bark_seed = a_rotation.w;

  vec3 model_pos = a_pos;

  float foliage_mask = smoothstep(0.34, 0.42, a_tex_coord.y);
  float tip_mask = smoothstep(0.88, 1.02, a_tex_coord.y);
  float angle = a_tex_coord.x * TWO_PI;

  float aspect_w = mix(0.72, 1.38, needle_seed);
  float aspect_h = mix(1.12, 0.84, needle_seed);
  model_pos.xz *= mix(1.0, aspect_w, foliage_mask);
  model_pos.y *= mix(1.0, aspect_h, foliage_mask);

  float irregular_base = sin(angle * 3.0 + silhouette_seed * TWO_PI);
  float irregular_mid = sin(angle * 5.0 + silhouette_seed * TWO_PI * 2.3 + 1.1);
  float irregular_fine = sin(angle * 7.0 + silhouette_seed * TWO_PI * 3.7 + 2.3);
  float irregular =
      (irregular_base * 0.22 + irregular_mid * 0.10 + irregular_fine * 0.05) *
      foliage_mask * (1.0 - tip_mask * 0.5);

  model_pos.xz *= (1.0 + irregular);

  float droop = foliage_mask * (1.0 - tip_mask) * 0.12;
  model_pos.y -= droop;

  float lean_norm = clamp(a_pos.y / 1.13, 0.0, 1.0);
  float lean_angle = (silhouette_seed - 0.5) * 0.18;
  float lean_yaw = bark_seed * TWO_PI;
  model_pos.x += cos(lean_yaw) * lean_angle * lean_norm;
  model_pos.z += sin(lean_yaw) * lean_angle * lean_norm;

  float height_factor = clamp(model_pos.y, 0.0, 1.1);
  vec3 local_pos = model_pos * scale;

  float sway = sin(u_time * u_wind_speed * 0.5 + sway_phase) * u_wind_strength * 0.8 *
               height_factor * height_factor;

  float sway_influence = mix(0.04, 0.12, foliage_mask);
  local_pos.x += sway * sway_influence;

  local_pos.y -= sway * 0.02 * foliage_mask;

  vec3 local_normal = a_normal;
  if (foliage_mask > 0.0) {
    float normal_scale = 1.0 + irregular;
    local_normal = normalize(vec3(local_normal.x * normal_scale,
                                 local_normal.y - foliage_mask * 0.2,
                                 local_normal.z * normal_scale));
  }

  float cos_r = cos(rotation);
  float sin_r = sin(rotation);
  mat2 rot = mat2(cos_r, -sin_r, sin_r, cos_r);

  vec2 rotated_xz = rot * local_pos.xz;
  local_pos = vec3(rotated_xz.x, local_pos.y, rotated_xz.y);

  vec2 rotated_normal_xz = rot * local_normal.xz;
  vec3 final_normal =
      normalize(vec3(rotated_normal_xz.x, local_normal.y, rotated_normal_xz.y));

  v_world_pos = local_pos + world_pos;
  v_normal = final_normal;
  v_color = a_color_sway.rgb;
  v_tex_coord = a_tex_coord;
  v_foliage_mask = foliage_mask;
  v_needle_seed = needle_seed;
  v_bark_seed = bark_seed;

  gl_Position = u_view_proj * vec4(v_world_pos, 1.0);
}
