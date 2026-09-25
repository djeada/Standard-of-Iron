#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_tex_coord;

layout(location = 3) in vec4 a_instance_model_col0;
layout(location = 4) in vec4 a_instance_model_col1;
layout(location = 5) in vec4 a_instance_model_col2;
layout(location = 6) in vec4 a_instance_color_alpha;

layout(std140) uniform FrameData {
  mat4 u_view_proj;
};

uniform int u_material_id;

out vec3 v_normal;
out vec2 v_tex_coord;
out vec3 v_world_pos;
flat out vec3 v_instance_color;
flat out float v_instance_alpha;
flat out int v_material_id;
flat out float v_ground_height;

const float k_no_ground_contact = -1.0e6;

vec3 soi_transform_normal(mat3 m, vec3 n) {
  mat3 cofactor = mat3(cross(m[1], m[2]), cross(m[2], m[0]), cross(m[0], m[1]));
  float handedness = dot(m[0], cross(m[1], m[2])) < 0.0 ? -1.0 : 1.0;
  return cofactor * n * handedness;
}

void main() {
  mat4 model = mat4(vec4(a_instance_model_col0.xyz, 0.0),
                    vec4(a_instance_model_col1.xyz, 0.0),
                    vec4(a_instance_model_col2.xyz, 0.0),
                    vec4(a_instance_model_col0.w,
                         a_instance_model_col1.w,
                         a_instance_model_col2.w,
                         1.0));

  vec4 world_pos4 = model * vec4(a_position, 1.0);
  v_world_pos = world_pos4.xyz;
  v_normal = soi_transform_normal(mat3(model), a_normal);
  v_tex_coord = a_tex_coord;
  v_instance_color = a_instance_color_alpha.rgb;
  v_instance_alpha = a_instance_color_alpha.a;
  v_material_id = u_material_id;
  v_ground_height = k_no_ground_contact;
  gl_Position = u_view_proj * world_pos4;
}
