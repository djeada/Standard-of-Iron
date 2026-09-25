#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_tex_coord;

layout(location = 3) in vec4 a_instance_model_col0;
layout(location = 4) in vec4 a_instance_model_col1;
layout(location = 5) in vec4 a_instance_model_col2;
layout(location = 6) in vec4 a_instance_palette0;
layout(location = 7) in vec4 a_instance_palette1;
layout(location = 8) in vec4 a_instance_state;

layout(location = 9) in vec4 a_part_color_alpha;
layout(location = 10) in vec4 a_part_material;

layout(std140) uniform FrameData {
  mat4 u_view_proj;
};

out vec3 v_normal;
out vec2 v_tex_coord;
out vec3 v_world_pos;
flat out vec3 v_instance_color;
flat out float v_instance_alpha;
flat out int v_material_id;

vec3 soi_transform_normal(mat3 m, vec3 n) {
  mat3 cofactor = mat3(cross(m[1], m[2]), cross(m[2], m[0]), cross(m[0], m[1]));
  float handedness = dot(m[0], cross(m[1], m[2])) < 0.0 ? -1.0 : 1.0;
  return cofactor * n * handedness;
}

vec3 soi_unseen_surface_color(vec3 color) {
  float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
  float grey = luminance * (1.0 - 0.80);
  return (vec3(grey) + color * 0.80) * vec3(0.56, 0.56, 0.58) +
         vec3(0.010, 0.010, 0.012);
}

void main() {
  mat4 model = mat4(vec4(a_instance_model_col0.xyz, 0.0),
                    vec4(a_instance_model_col1.xyz, 0.0),
                    vec4(a_instance_model_col2.xyz, 0.0),
                    vec4(a_instance_model_col0.w,
                         a_instance_model_col1.w,
                         a_instance_model_col2.w,
                         1.0));

  int slot = int(a_part_material.x);
  bool unseen = a_instance_state.y > 0.5;
  vec4 palette = slot == 0 ? a_instance_palette0 : a_instance_palette1;
  int base_material = int(a_part_material.w);
  int material;
  if (slot >= 0 && palette.w >= 0.0) {
    v_instance_color = palette.rgb;
    material = base_material;
    if (material % 10 == 0) {
      material += int(palette.w);
    }
  } else {
    v_instance_color = unseen ? soi_unseen_surface_color(a_part_color_alpha.rgb)
                              : a_part_color_alpha.rgb;
    material = int(unseen ? a_part_material.z : a_part_material.y);
  }
  if (base_material < 10) {
    material += int(a_instance_state.x);
  }
  v_material_id = material;
  v_instance_alpha = a_part_color_alpha.a;

  vec4 world_pos4 = model * vec4(a_position, 1.0);
  v_world_pos = world_pos4.xyz;
  v_normal = soi_transform_normal(mat3(model), a_normal);
  v_tex_coord = a_tex_coord;
  gl_Position = u_view_proj * world_pos4;
}
