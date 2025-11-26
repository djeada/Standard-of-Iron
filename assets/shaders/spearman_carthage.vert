#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

uniform mat4 u_mvp;
uniform mat4 u_model;

out vec3 v_worldPos;
out vec3 v_normal;
out vec2 v_texCoord;
out float v_height;

void main() {
    vec4 worldPos4 = u_model * vec4(a_position, 1.0);
    v_worldPos = worldPos4.xyz;

    mat3 normalMatrix = mat3(u_model);
    v_normal = normalize(normalMatrix * a_normal);

    v_texCoord = a_texCoord;
    v_height = a_position.y;

    gl_Position = u_mvp * vec4(a_position, 1.0);
}
