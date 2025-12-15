#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_offset;
layout(location = 2) in float a_alpha;

uniform mat4 u_view_proj;
uniform float u_time;
uniform float u_intensity;
uniform vec3 u_camera_pos;
uniform vec3 u_wind;

out float v_alpha;

const float AREA_HEIGHT = 30.0;
const float AREA_RADIUS = 50.0;

void main() {
  float speed = a_offset.z;
  float y_offset = a_offset.y;

  float fall_distance = mod(speed * u_time, AREA_HEIGHT);

  vec3 pos = a_position;
  pos.x += u_camera_pos.x;
  pos.z += u_camera_pos.z;

  pos.y = pos.y - fall_distance + y_offset;

  if (pos.y < 0.0) {
    pos.y += AREA_HEIGHT;
  }

  pos.x += u_wind.x * (AREA_HEIGHT - pos.y) * 0.1;
  pos.z += u_wind.z * (AREA_HEIGHT - pos.y) * 0.1;

  gl_Position = u_view_proj * vec4(pos, 1.0);

  v_alpha = a_alpha * u_intensity;
}
