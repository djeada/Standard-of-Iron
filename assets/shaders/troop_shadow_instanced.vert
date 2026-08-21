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

out vec2 v_tex_coord;
out vec3 v_world_pos;
out float v_alpha;

void main() {
  mat4 model = mat4(vec4(a_instance_model_col0.xyz, 0.0),
                    vec4(a_instance_model_col1.xyz, 0.0),
                    vec4(a_instance_model_col2.xyz, 0.0),
                    vec4(a_instance_model_col0.w,
                         a_instance_model_col1.w,
                         a_instance_model_col2.w,
                         1.0));
  vec4 world = model * vec4(a_position, 1.0);
  v_tex_coord = a_tex_coord;
  v_world_pos = world.xyz;
  v_alpha = a_instance_color_alpha.a;
  gl_Position = u_view_proj * world;
}
