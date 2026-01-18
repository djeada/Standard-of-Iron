#version 330 core

in vec2 v_uv;
in vec2 v_world_pos;

uniform vec4 u_color;
uniform vec4 u_hover_color;
uniform float u_hover_blend;
uniform float u_time;

uniform sampler2D u_parchment_texture;
uniform bool u_use_parchment;
uniform float u_parchment_scale;
uniform float u_parchment_strength;

uniform bool u_is_hovered;
uniform float u_pulse_speed;
uniform float u_pulse_amplitude;

out vec4 fragColor;

// Procedural noise functions
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453123);
}

float valueNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

// FBM for organic textures
float fbm(vec2 p, int octaves) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    
    for (int i = 0; i < octaves; i++) {
        value += amplitude * valueNoise(p * frequency);
        amplitude *= 0.5;
        frequency *= 2.0;
    }
    
    return value;
}

// Parchment/ink mask texture
float getParchmentMask(vec2 uv) {
    // Multi-octave noise for natural paper texture
    float n1 = fbm(uv * u_parchment_scale, 4);
    float n2 = fbm(uv * u_parchment_scale * 2.5 + vec2(100.0), 3);
    float n3 = fbm(uv * u_parchment_scale * 5.0 + vec2(200.0), 2);
    
    // Combine different scales for rich texture
    float combined = n1 * 0.5 + n2 * 0.35 + n3 * 0.15;
    
    // Map to subtle intensity variation
    return 0.85 + combined * 0.15;
}

// Edge darkening for engraved look
float getEdgeDarkening(vec2 uv) {
    // Use noise-based edge detection approximation
    float center = fbm(uv * 20.0, 2);
    float right = fbm((uv + vec2(0.002, 0.0)) * 20.0, 2);
    float up = fbm((uv + vec2(0.0, 0.002)) * 20.0, 2);
    
    float edge = abs(center - right) + abs(center - up);
    return 1.0 - edge * 0.3;
}

void main() {
    vec4 color = u_color;
    
    // Apply hover effect with pulsing
    if (u_is_hovered) {
        float pulse = 0.5 + 0.5 * sin(u_time * u_pulse_speed);
        float brightness = u_pulse_amplitude * pulse;
        
        // Blend towards hover color with pulsing brightness
        vec4 hoverEffect = mix(color, u_hover_color, u_hover_blend);
        hoverEffect.rgb += vec3(brightness);
        
        color = hoverEffect;
    }
    
    // Apply parchment/ink texture
    if (u_use_parchment) {
        float parchment;
        
        // Try to sample texture, fall back to procedural
        if (textureSize(u_parchment_texture, 0).x > 1) {
            vec2 texUV = v_uv * u_parchment_scale;
            parchment = texture(u_parchment_texture, texUV).r;
        } else {
            parchment = getParchmentMask(v_uv);
        }
        
        // Blend with multiply mode for authentic look
        float texStrength = u_parchment_strength;
        color.rgb *= mix(1.0, parchment, texStrength);
        
        // Subtle edge darkening for depth
        float edge = getEdgeDarkening(v_uv);
        color.rgb *= mix(1.0, edge, texStrength * 0.5);
    }
    
    // Apply owner tint as overlay blend
    // This allows the parchment texture to show through while tinting
    vec3 tintedColor = color.rgb;
    
    // Slight vignette within province for depth
    float distFromCenter = length(v_uv - vec2(0.5));
    float provinceVignette = 1.0 - distFromCenter * distFromCenter * 0.1;
    color.rgb *= provinceVignette;
    
    // Output with premultiplied alpha
    fragColor = vec4(color.rgb * color.a, color.a);
}
