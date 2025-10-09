#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

layout(location = 3) in vec3 i_center;
layout(location = 4) in float i_size;
layout(location = 5) in vec3 i_color;
layout(location = 6) in float i_alpha;

uniform mat4 u_viewProj;

out vec3 v_worldPos;
out vec3 v_normal;
out vec3 v_color;
out float v_alpha;

void main() {
    vec3 worldPos = vec3(i_center.x + a_position.x * i_size,
                         i_center.y + a_position.y,
                         i_center.z + a_position.z * i_size);

    v_worldPos = worldPos;
    v_normal = vec3(0.0, 1.0, 0.0);
    v_color = i_color;
    v_alpha = i_alpha;

    gl_Position = u_viewProj * vec4(worldPos, 1.0);
}
