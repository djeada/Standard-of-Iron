#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_tex_coord;

layout(location = 3) in vec3 i_start;
layout(location = 4) in vec3 i_end;
layout(location = 5) in float i_radius;
layout(location = 6) in float i_alpha;
layout(location = 7) in vec3 i_color;

layout(std140) uniform FrameData { mat4 u_view_proj; };

out vec3 v_world_pos;
out vec3 v_normal;
out vec3 v_color;
out float v_alpha;

const float EPSILON = 1e-5;

void main() {
  vec3 axis = i_end - i_start;
  float len = length(axis);
  vec3 dir = len > EPSILON ? axis / len : vec3(0.0, 1.0, 0.0);

  vec3 up = abs(dir.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
  vec3 tangent = normalize(cross(up, dir));
  if (length(tangent) < EPSILON) {
    tangent = vec3(1.0, 0.0, 0.0);
  }
  vec3 bitangent = cross(dir, tangent);
  if (length(bitangent) < EPSILON) {
    bitangent = vec3(0.0, 0.0, 1.0);
  } else {
    bitangent = normalize(bitangent);
  }
  tangent = normalize(cross(bitangent, dir));

  vec3 local_pos = a_position;
  float along = (local_pos.y + 0.5) * len;
  vec3 radial = tangent * local_pos.x + bitangent * local_pos.z;

  vec3 world_pos = i_start + dir * along + radial * i_radius;

  vec3 local_normal = a_normal;
  vec3 world_normal = normalize(tangent * local_normal.x + dir * local_normal.y +
                               bitangent * local_normal.z);

  v_world_pos = world_pos;
  v_normal = world_normal;
  v_color = i_color;
  v_alpha = i_alpha;

  gl_Position = u_view_proj * vec4(world_pos, 1.0);
}
