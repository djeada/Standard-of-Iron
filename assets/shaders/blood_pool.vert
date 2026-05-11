#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 2) in vec2 a_texCoord;

uniform mat4 u_mvp;
out vec2 v_uv;

void main() {
    v_uv = a_texCoord;
    gl_Position = u_mvp * vec4(a_position, 1.0);
}
