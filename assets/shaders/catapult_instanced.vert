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

out vec3 v_normal;
out vec2 v_tex_coord;
out vec3 v_world_pos;
flat out vec3 v_instance_color;
flat out float v_instance_alpha;
out float v_material_region;

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
  v_normal = mat3(model) * a_normal;
  v_tex_coord = a_tex_coord;
  v_instance_color = a_instance_color_alpha.rgb;
  v_instance_alpha = a_instance_color_alpha.a;
  gl_Position = u_view_proj * world_pos4;

  if (v_world_pos.y < 0.25) {
    v_material_region = 0.0;
  } else if (v_world_pos.y < 0.55) {
    v_material_region = 1.0;
  } else {
    v_material_region = 2.0;
  }
}
