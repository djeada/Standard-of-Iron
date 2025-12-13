#version 330 core

in vec3 v_worldPos;
in vec3 v_normal;
in vec2 v_texCoord;
in float v_beamT;
in float v_edgeDist;
in float v_glowIntensity;

uniform float u_time;
uniform float u_progress;
uniform vec3 u_healColor;      // Base healing color (golden-green)
uniform float u_alpha;

out vec4 FragColor;

// Pseudo-random for noise effects
float hash(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// Fractal brownian motion for magical swirling
float fbm(vec2 p) {
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 4; i++) {
        value += amplitude * noise(p);
        p *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

void main() {
    // Base magical colors: golden-green healing energy
    vec3 coreColor = vec3(0.95, 0.95, 0.6);      // Bright golden-white core
    vec3 innerColor = vec3(0.4, 1.0, 0.5);       // Vibrant green
    vec3 outerColor = vec3(0.2, 0.8, 0.3);       // Deeper green edge
    vec3 sparkleColor = vec3(1.0, 1.0, 0.8);     // Sparkle highlights
    
    // Mix with provided heal color
    innerColor = mix(innerColor, u_healColor, 0.5);
    outerColor = mix(outerColor, u_healColor * 0.7, 0.5);
    
    // Distance from beam center affects color
    float centerDist = v_edgeDist;
    
    // Core glow - brightest at center
    float coreGlow = exp(-centerDist * 8.0);
    
    // Inner aura
    float innerGlow = exp(-centerDist * 3.0);
    
    // Outer aura
    float outerGlow = exp(-centerDist * 1.5);
    
    // Combine color layers
    vec3 color = outerColor * outerGlow;
    color = mix(color, innerColor, innerGlow);
    color = mix(color, coreColor, coreGlow);
    
    // Add magical sparkles
    vec2 sparkleUV = v_texCoord * 40.0 + vec2(u_time * 3.0, 0.0);
    float sparkle = noise(sparkleUV) * noise(sparkleUV * 1.5);
    sparkle = pow(sparkle, 4.0) * 2.0;
    color += sparkleColor * sparkle * innerGlow;
    
    // Flowing energy pattern
    vec2 flowUV = vec2(v_beamT * 10.0 - u_time * 4.0, centerDist * 5.0);
    float flow = fbm(flowUV);
    color += innerColor * flow * 0.3 * innerGlow;
    
    // Pulsing glow
    float pulse = 0.8 + 0.2 * sin(u_time * 8.0 + v_beamT * 15.0);
    color *= pulse;
    
    // Edge shimmer (magical energy at edges)
    float edgeShimmer = sin(v_beamT * 50.0 - u_time * 10.0) * 0.5 + 0.5;
    edgeShimmer *= (1.0 - innerGlow) * outerGlow;
    color += sparkleColor * edgeShimmer * 0.2;
    
    // Alpha based on glow intensity and visibility
    float alpha = v_glowIntensity * outerGlow * u_alpha;
    
    // Fade in at start, fade out at end of beam
    float startFade = smoothstep(0.0, 0.1, v_beamT);
    float endFade = smoothstep(u_progress, u_progress - 0.1, v_beamT);
    alpha *= startFade * endFade;
    
    // Impact burst at target (when beam reaches end)
    if (v_beamT > 0.9 && u_progress > 0.95) {
        float impactGlow = (v_beamT - 0.9) * 10.0;
        impactGlow *= sin(u_time * 15.0) * 0.5 + 0.5;
        color += coreColor * impactGlow * 0.5;
        alpha += impactGlow * 0.3;
    }
    
    // Bloom effect simulation (bright pixels)
    float bloom = max(0.0, (coreGlow - 0.5) * 2.0);
    color += coreColor * bloom * 0.3;
    
    FragColor = vec4(color, clamp(alpha, 0.0, 1.0));
}
