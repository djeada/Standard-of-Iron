#version 330 core

layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in float a_height;
layout(location = 3) in vec3 a_normal;

uniform mat4 u_mvp;
uniform float u_height_scale;
uniform float u_z_base;

out vec2 v_uv;
out vec3 v_normal;
out vec3 v_world_pos;
out float v_height;

void main() {
    // Apply height displacement to create 3D terrain
    float displaced_height = u_z_base + a_height * u_height_scale;
    
    // World position with height-displaced Y coordinate
    vec3 world = vec3(1.0 - a_pos.x, displaced_height, a_pos.y);
    
    gl_Position = u_mvp * vec4(world, 1.0);
    
    v_uv = a_pos;
    v_normal = normalize(a_normal);
    v_world_pos = world;
    v_height = a_height;
}
