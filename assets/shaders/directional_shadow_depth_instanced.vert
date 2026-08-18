#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 3) in vec4 a_instance_model_col0;
layout(location = 4) in vec4 a_instance_model_col1;
layout(location = 5) in vec4 a_instance_model_col2;
uniform mat4 u_light_vp;
void main() {
  mat4 model = mat4(vec4(a_instance_model_col0.xyz, 0.0),
                    vec4(a_instance_model_col1.xyz, 0.0),
                    vec4(a_instance_model_col2.xyz, 0.0),
                    vec4(a_instance_model_col0.w,
                         a_instance_model_col1.w,
                         a_instance_model_col2.w,
                         1.0));
  gl_Position = u_light_vp * model * vec4(a_position, 1.0);
}
