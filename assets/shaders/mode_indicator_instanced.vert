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

uniform float u_time;

flat out vec3 v_instance_color;
flat out float v_instance_alpha;
out vec3 v_normal;
out float v_layer;
out float v_radial;
out float v_phase;
out float v_height;

void main() {
  mat4 model = mat4(vec4(a_instance_model_col0.xyz, 0.0),
                    vec4(a_instance_model_col1.xyz, 0.0),
                    vec4(a_instance_model_col2.xyz, 0.0),
                    vec4(a_instance_model_col0.w,
                         a_instance_model_col1.w,
                         a_instance_model_col2.w,
                         1.0));

  vec3 origin =
      vec3(a_instance_model_col0.w, a_instance_model_col1.w, a_instance_model_col2.w);
  v_phase = origin.x * 0.9 + origin.z * 1.7;

  v_normal = a_normal;
  v_layer = a_tex_coord.x;
  v_radial = a_tex_coord.y;
  v_instance_color = a_instance_color_alpha.rgb;
  v_instance_alpha = a_instance_color_alpha.a;

  float pulse = 1.0 + 0.015 * sin(u_time * 2.0 + v_phase);
  vec3 pos = vec3(a_position.xy * pulse, a_position.z);
  v_height = a_position.y * 2.0;

  gl_Position = u_view_proj * model * vec4(pos, 1.0);
}
