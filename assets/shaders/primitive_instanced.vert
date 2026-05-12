#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_tex_coord;

layout(location = 3) in vec4 i_model_col0;
layout(location = 4) in vec4 i_model_col1;
layout(location = 5) in vec4 i_model_col2;
layout(location = 6) in vec4 i_color_alpha;

layout(std140) uniform FrameData { mat4 u_view_proj; };
uniform vec3 u_light_dir;

out vec3 v_world_pos;
out vec3 v_normal;
out vec3 v_color;
out float v_alpha;

void main() {

  mat4 model_matrix =
      mat4(vec4(i_model_col0.xyz, 0.0), vec4(i_model_col1.xyz, 0.0),
           vec4(i_model_col2.xyz, 0.0),
           vec4(i_model_col0.w, i_model_col1.w, i_model_col2.w, 1.0));

  vec4 world_pos4 = model_matrix * vec4(a_position, 1.0);
  v_world_pos = world_pos4.xyz;

  mat3 normal_matrix = mat3(model_matrix);

  v_normal = normalize(normal_matrix * a_normal);

  v_color = i_color_alpha.rgb;
  v_alpha = i_color_alpha.a;

  gl_Position = u_view_proj * world_pos4;
}
