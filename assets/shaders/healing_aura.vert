#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

uniform mat4 u_mvp;
uniform mat4 u_model;
uniform float u_time;
uniform float u_auraRadius;
uniform float u_intensity;     // 0.0-1.0 healing intensity

out vec3 v_worldPos;
out vec3 v_normal;
out vec2 v_texCoord;
out float v_height;
out float v_radialDist;

void main() {
    vec3 pos = a_position;
    
    // Scale to aura radius
    pos.xz *= u_auraRadius;
    
    // Height is based on original Y, used for vertical gradient
    v_height = a_position.y;
    
    // Radial distance from center
    v_radialDist = length(a_position.xz);
    
    // Gentle breathing/pulsing animation
    float breathe = 1.0 + 0.05 * sin(u_time * 2.0);
    pos.xz *= breathe;
    
    // Rising spiral motion
    float spiralAngle = atan(pos.z, pos.x);
    float spiralOffset = sin(spiralAngle * 3.0 + u_time * 2.0 + v_height * 5.0) * 0.1;
    pos.xz *= (1.0 + spiralOffset * u_intensity);
    
    // Gentle vertical wave
    float wave = sin(v_radialDist * 10.0 - u_time * 3.0) * 0.05 * u_intensity;
    pos.y += wave;
    
    v_worldPos = vec3(u_model * vec4(pos, 1.0));
    v_normal = mat3(u_model) * a_normal;
    v_texCoord = a_texCoord;
    
    gl_Position = u_mvp * vec4(pos, 1.0);
}
