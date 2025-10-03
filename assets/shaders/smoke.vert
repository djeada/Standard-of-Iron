#version 330 core

layout (location = 0) in vec3 a_position; // quad unit square in [-1,1] XY, Z ignored
layout (location = 2) in vec2 a_texCoord; // reuse for UV

uniform mat4 u_view;
uniform mat4 u_projection;
uniform vec3 u_camRight;   // camera right (world)
uniform vec3 u_camUp;      // camera up (world)
uniform vec3 u_center;     // particle center in world
uniform float u_size;      // particle half-size (world)
uniform float u_height;    // Y offset above ground
uniform mat4 u_model;      // overall model (for group translation/scale in XZ)

out vec2 v_uv;
out float v_soft;

void main() {
    // Billboard quad offset in world space using cylindrical billboarding (XZ facing)
    vec2 uv = a_texCoord * 2.0 - 1.0; // to [-1,1]
    vec3 right = normalize(u_camRight);
    vec3 up = normalize(u_camUp);
    vec3 worldCenter = (u_model * vec4(u_center.x, u_height, u_center.z, 1.0)).xyz;
    vec3 worldPos = worldCenter + (right * uv.x + up * uv.y) * u_size;
    gl_Position = u_projection * u_view * vec4(worldPos, 1.0);
    v_uv = uv;
    v_soft = abs(uv.y);
}
