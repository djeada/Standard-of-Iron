#version 330 core

layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec2 a_local;

uniform mat4 u_mvp;
uniform float u_z;
uniform float u_scale;
uniform vec2 u_anchor;  // Label anchor point for scaling

out vec2 v_uv;
out vec2 v_local;
out vec2 v_world_pos;

void main() {
    // Apply scale for zoom-consistent sizing around anchor point
    vec2 scaled_pos = u_anchor + (a_pos - u_anchor) * u_scale;
    
    vec3 world = vec3(1.0 - scaled_pos.x, u_z, scaled_pos.y);
    gl_Position = u_mvp * vec4(world, 1.0);
    
    v_uv = a_uv;
    v_local = a_local;
    v_world_pos = scaled_pos;
}
