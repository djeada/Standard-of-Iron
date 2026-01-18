#version 330 core

layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_local;

uniform mat4 u_mvp;
uniform vec2 u_screen_pos;
uniform float u_size;
uniform float u_z;

out vec2 v_local;
out vec2 v_uv;

void main() {
    // Convert from local badge coordinates to world position
    vec2 worldPos = u_screen_pos + a_local * u_size;
    vec3 world = vec3(1.0 - worldPos.x, u_z, worldPos.y);
    
    gl_Position = u_mvp * vec4(world, 1.0);
    
    v_local = a_local;
    v_uv = a_local * 0.5 + 0.5; // Normalize to [0,1]
}
