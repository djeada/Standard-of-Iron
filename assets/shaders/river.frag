#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform float time;

// Simple hash for procedural water texture
float hash21(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise21(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  
  float a = hash21(i);
  float b = hash21(i + vec2(1.0, 0.0));
  float c = hash21(i + vec2(0.0, 1.0));
  float d = hash21(i + vec2(1.0, 1.0));
  
  vec2 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

void main() {
    // Animate UVs for water flow
    vec2 flowUV = vec2(TexCoord.x + time * 0.05, TexCoord.y);
    
    // Create procedural water texture with multiple layers
    float n1 = noise21(flowUV * 8.0);
    float n2 = noise21(flowUV * 16.0 + vec2(time * 0.1, 0.0));
    float waterPattern = n1 * 0.6 + n2 * 0.4;
    
    // Base water color (blue-green tint)
    vec3 waterColor = vec3(0.2, 0.4, 0.6) + vec3(waterPattern * 0.2);
    
    // Add slight transparency and color tint
    FragColor = vec4(waterColor * 0.8 + vec3(0.0, 0.05, 0.1), 0.9);
}
