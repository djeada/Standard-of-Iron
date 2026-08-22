#version 330 core

layout(location = 0) in vec3 a_position;

uniform mat4 u_sky_box_view_proj;

out vec3 v_direction;

void main() {
  v_direction = a_position;
  vec4 clip = u_sky_box_view_proj * vec4(a_position, 1.0);

  gl_Position = clip.xyww;
}
