#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

layout(location = 3) in vec3 i_start;
layout(location = 4) in vec3 i_end;
layout(location = 5) in float i_radius;
layout(location = 6) in float i_alpha;
layout(location = 7) in vec3 i_color;

uniform mat4 u_viewProj;

out vec3 v_worldPos;
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

  vec3 localPos = a_position;
  float along = (localPos.y + 0.5) * len;
  vec3 radial = tangent * localPos.x + bitangent * localPos.z;

  vec3 worldPos = i_start + dir * along + radial * i_radius;

  vec3 localNormal = a_normal;
  vec3 worldNormal = normalize(tangent * localNormal.x + dir * localNormal.y +
                               bitangent * localNormal.z);

  v_worldPos = worldPos;
  v_normal = worldNormal;
  v_color = i_color;
  v_alpha = i_alpha;

  gl_Position = u_viewProj * vec4(worldPos, 1.0);
}
