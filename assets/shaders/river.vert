#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float time;

out vec2 TexCoord;

void main() {
    // Slight vertex wave to simulate water movement
    vec3 pos = aPos;
    pos.y += sin(aPos.x * 0.1 + time * 0.8) * 0.05;
    gl_Position = projection * view * model * vec4(pos, 1.0);
    TexCoord = aTexCoord;
}
