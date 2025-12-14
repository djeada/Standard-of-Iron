#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texcoord;

uniform mat4 u_mvp;
uniform mat4 u_model;
uniform float u_time;
uniform vec3 u_center;
uniform float u_radius;
uniform float u_intensity;

out vec3 v_world_pos;
out vec3 v_normal;
out vec2 v_texcoord;
out float v_intensity;
out float v_alpha;

void main() {
    vec3 pos = a_position;
    
    // Calculate radial position from center
    vec3 world_pos = (u_model * vec4(pos, 1.0)).xyz;
    vec3 to_center = world_pos - u_center;
    float dist = length(to_center.xz);
    float normalized_dist = dist / max(u_radius, 0.001);
    
    // Add swirling motion
    float swirl_angle = u_time * 1.5 + normalized_dist * 3.14159;
    float swirl_strength = 0.15 * (1.0 - normalized_dist);
    pos.x += sin(swirl_angle) * swirl_strength;
    pos.z += cos(swirl_angle) * swirl_strength;
    
    // Add vertical bobbing
    float bob = sin(u_time * 2.0 + pos.x * 3.0) * 0.05;
    pos.y += bob + 0.1 * sin(u_time * 1.5 + pos.z * 2.0);
    
    // Rise over time then fade
    float rise = max(0.0, sin(u_time * 0.5 + normalized_dist * 2.0)) * 0.3;
    pos.y += rise;
    
    // Calculate alpha based on distance from center and time
    float edge_fade = smoothstep(1.0, 0.7, normalized_dist);
    float time_pulse = 0.7 + 0.3 * sin(u_time * 1.5);
    v_alpha = edge_fade * time_pulse * u_intensity;
    
    v_world_pos = (u_model * vec4(pos, 1.0)).xyz;
    v_normal = normalize(mat3(u_model) * a_normal);
    v_texcoord = a_texcoord;
    v_intensity = u_intensity;
    
    gl_Position = u_mvp * vec4(pos, 1.0);
}
