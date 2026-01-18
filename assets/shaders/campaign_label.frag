#version 330 core

in vec2 v_uv;
in vec2 v_local;
in vec2 v_world_pos;

uniform sampler2D u_font_atlas;
uniform vec4 u_fill_color;
uniform vec4 u_stroke_color;
uniform float u_stroke_width;
uniform float u_smoothing;

uniform bool u_use_sdf;
uniform bool u_use_stroke;

out vec4 fragColor;

// SDF-based text rendering with stroke support
float sampleSDF(vec2 uv) {
    // Sample distance field from atlas
    float dist = texture(u_font_atlas, uv).r;
    
    // Convert from [0, 1] to signed distance
    return (dist - 0.5) * 2.0;
}

void main() {
    if (u_use_sdf) {
        // SDF-based rendering for crisp text at any scale
        float dist = sampleSDF(v_uv);
        
        // Anti-aliased edge
        float alpha = smoothstep(-u_smoothing, u_smoothing, dist);
        
        vec4 color = u_fill_color;
        color.a *= alpha;
        
        // Optional stroke/outline
        if (u_use_stroke && u_stroke_width > 0.0) {
            float strokeDist = dist + u_stroke_width * 0.1;
            float strokeAlpha = smoothstep(-u_smoothing, u_smoothing, strokeDist);
            
            // Blend fill over stroke
            vec4 strokeColor = u_stroke_color;
            strokeColor.a *= strokeAlpha;
            
            // Composite: stroke underneath, fill on top
            color = mix(strokeColor, color, alpha);
            color.a = max(strokeColor.a, color.a * alpha);
        }
        
        fragColor = color;
    } else {
        // Simple bitmap text fallback
        vec4 texColor = texture(u_font_atlas, v_uv);
        
        vec4 color = u_fill_color;
        color.a *= texColor.a;
        
        // Simple stroke using texture sampling
        if (u_use_stroke && u_stroke_width > 0.0) {
            float strokeOffset = u_stroke_width * 0.002;
            
            // Sample neighbors for outline
            float neighbors = 0.0;
            neighbors += texture(u_font_atlas, v_uv + vec2(strokeOffset, 0.0)).a;
            neighbors += texture(u_font_atlas, v_uv - vec2(strokeOffset, 0.0)).a;
            neighbors += texture(u_font_atlas, v_uv + vec2(0.0, strokeOffset)).a;
            neighbors += texture(u_font_atlas, v_uv - vec2(0.0, strokeOffset)).a;
            neighbors *= 0.25;
            
            vec4 strokeColor = u_stroke_color;
            strokeColor.a *= neighbors;
            
            // Composite
            color = mix(strokeColor, color, texColor.a);
            color.a = max(strokeColor.a, color.a);
        }
        
        fragColor = color;
    }
    
    // Discard fully transparent fragments
    if (fragColor.a < 0.01) {
        discard;
    }
}
