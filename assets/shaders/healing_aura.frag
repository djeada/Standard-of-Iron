#version 330 core

in vec3 v_worldPos;
in vec3 v_normal;
in vec2 v_texCoord;
in float v_height;
in float v_radialDist;

uniform float u_time;
uniform float u_intensity;
uniform vec3 u_auraColor;

out vec4 FragColor;

float hash(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x),
               mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x), f.y);
}

void main() {
    // Base magical colors
    vec3 coreColor = vec3(1.0, 1.0, 0.7);        // Golden-white center
    vec3 midColor = u_auraColor;                  // Main heal color
    vec3 edgeColor = u_auraColor * 0.5;          // Darker edge
    
    // Radial gradient: bright at center, fading outward
    float radialFade = 1.0 - smoothstep(0.0, 1.0, v_radialDist);
    
    // Height gradient: stronger at bottom, fading upward
    float heightFade = 1.0 - smoothstep(0.0, 1.5, v_height);
    
    // Combined falloff
    float falloff = radialFade * heightFade;
    
    // Color gradient from center to edge
    vec3 color = mix(edgeColor, midColor, radialFade);
    color = mix(color, coreColor, radialFade * radialFade * 0.5);
    
    // Magical swirling pattern
    float angle = atan(v_worldPos.z, v_worldPos.x);
    float swirl = sin(angle * 4.0 + u_time * 2.0 + v_radialDist * 8.0) * 0.5 + 0.5;
    color += coreColor * swirl * 0.15 * falloff;
    
    // Rising particles effect
    vec2 particleUV = vec2(angle * 2.0, v_height * 3.0 - u_time * 1.5);
    float particles = noise(particleUV * 8.0);
    particles = pow(particles, 3.0) * 3.0;
    color += coreColor * particles * falloff * 0.3;
    
    // Pulsing rings
    float ring = sin(v_radialDist * 20.0 - u_time * 4.0) * 0.5 + 0.5;
    ring = pow(ring, 4.0);
    color += midColor * ring * 0.2 * heightFade;
    
    // Outer shimmer
    float shimmer = noise(vec2(angle * 10.0, u_time * 2.0)) * (1.0 - radialFade);
    color += coreColor * shimmer * 0.1;
    
    // Alpha: visible where there's effect, transparent otherwise
    float alpha = falloff * u_intensity * 0.6;
    
    // Add extra alpha for particles
    alpha += particles * falloff * u_intensity * 0.2;
    
    // Soft edge
    alpha *= smoothstep(1.0, 0.7, v_radialDist);
    
    FragColor = vec4(color, clamp(alpha, 0.0, 0.8));
}
