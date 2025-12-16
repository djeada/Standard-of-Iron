#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_offset;
layout(location = 2) in float a_alpha;

uniform mat4 u_view_proj;
uniform float u_time;
uniform float u_intensity;
uniform vec3 u_camera_pos;
uniform vec3 u_wind;
uniform int u_weather_type;
uniform float u_wind_strength;

out float v_alpha;
out float v_rotation;

const float AREA_HEIGHT = 30.0;
const float AREA_RADIUS = 50.0;

const float RAIN_WIND_FACTOR = 0.1;
const float SNOW_WIND_FACTOR = 0.25;

const float SNOW_DRIFT_FREQ_X = 0.5;
const float SNOW_DRIFT_FREQ_Z = 0.3;
const float SNOW_DRIFT_SCALE_X = 0.3;
const float SNOW_DRIFT_AMPLITUDE_X = 0.15;
const float SNOW_DRIFT_AMPLITUDE_Z = 0.1;

const float SNOW_POINT_SIZE = 22.0;

void main() {
  float speed = a_offset.z;
  if (u_weather_type == 1) {
    speed *= 0.15;
  }
  float y_offset = a_offset.y;

  float fall_distance = mod(speed * u_time, AREA_HEIGHT);

  vec3 pos = a_position;
  pos.x += u_camera_pos.x;
  pos.z += u_camera_pos.z;

  pos.y = pos.y - fall_distance + y_offset;

  if (pos.y < 0.0) {
    pos.y += AREA_HEIGHT;
  }

  float wind_factor =
      (u_weather_type == 1) ? SNOW_WIND_FACTOR : RAIN_WIND_FACTOR;
  float height_factor = (AREA_HEIGHT - pos.y) / AREA_HEIGHT;

  pos.x += u_wind.x * height_factor * wind_factor * (1.0 + u_wind_strength);
  pos.z += u_wind.z * height_factor * wind_factor * (1.0 + u_wind_strength);

  if (u_weather_type == 1) {
    float drift =
        sin(u_time * SNOW_DRIFT_FREQ_X + a_position.x * SNOW_DRIFT_SCALE_X) *
        SNOW_DRIFT_AMPLITUDE_X;
    pos.x += drift;
    pos.z +=
        cos(u_time * SNOW_DRIFT_FREQ_Z + a_position.z * SNOW_DRIFT_SCALE_X) *
        SNOW_DRIFT_AMPLITUDE_Z;

    gl_PointSize = SNOW_POINT_SIZE;

    v_rotation = a_position.x * 2.0 + a_position.z * 3.0 + u_time * 0.2;
  }

  gl_Position = u_view_proj * vec4(pos, 1.0);

  v_alpha = a_alpha * u_intensity;
}
