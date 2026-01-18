#version 330 core

in vec2 v_local;
in float v_symbol_type;

uniform vec4 u_fill_color;
uniform vec4 u_stroke_color;
uniform vec4 u_shadow_color;
uniform float u_stroke_width;
uniform float u_shadow_offset;
uniform bool u_show_shadow;

out vec4 fragColor;

// Symbol type constants
const float SYMBOL_MOUNTAIN = 0.0;
const float SYMBOL_CITY = 1.0;
const float SYMBOL_PORT = 2.0;
const float SYMBOL_FORT = 3.0;
const float SYMBOL_TEMPLE = 4.0;

// SDF primitives
float sdTriangle(vec2 p, vec2 a, vec2 b, vec2 c) {
    vec2 e0 = b - a, e1 = c - b, e2 = a - c;
    vec2 v0 = p - a, v1 = p - b, v2 = p - c;
    
    vec2 pq0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), 0.0, 1.0);
    vec2 pq1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), 0.0, 1.0);
    vec2 pq2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), 0.0, 1.0);
    
    float s = sign(e0.x * e2.y - e0.y * e2.x);
    vec2 d = min(min(vec2(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)),
                     vec2(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))),
                     vec2(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)));
    
    return -sqrt(d.x) * sign(d.y);
}

float sdBox(vec2 p, vec2 b) {
    vec2 d = abs(p) - b;
    return length(max(d, vec2(0.0))) + min(max(d.x, d.y), 0.0);
}

float sdCircle(vec2 p, float r) {
    return length(p) - r;
}

// Mountain symbol - triangular peaks
float sdMountain(vec2 p) {
    // Main peak
    float peak1 = sdTriangle(p, 
        vec2(-0.35, 0.35), 
        vec2(0.0, -0.4), 
        vec2(0.35, 0.35));
    
    // Secondary peak (smaller, offset)
    vec2 p2 = p - vec2(0.25, 0.1);
    float peak2 = sdTriangle(p2,
        vec2(-0.2, 0.25),
        vec2(0.0, -0.2),
        vec2(0.2, 0.25));
    
    // Snow cap (subtraction from top)
    float snowLine = p.y + 0.15;
    
    return min(peak1, peak2);
}

// City symbol - building silhouette
float sdCity(vec2 p) {
    // Base rectangle
    float base = sdBox(p - vec2(0.0, 0.15), vec2(0.35, 0.2));
    
    // Central tower (taller)
    float tower = sdBox(p - vec2(0.0, -0.15), vec2(0.1, 0.35));
    
    // Side buildings
    float left = sdBox(p - vec2(-0.2, 0.0), vec2(0.1, 0.25));
    float right = sdBox(p - vec2(0.2, 0.0), vec2(0.1, 0.2));
    
    return min(min(base, tower), min(left, right));
}

// Port/anchor symbol
float sdPort(vec2 p) {
    // Anchor ring
    float ring = abs(length(p - vec2(0.0, -0.3)) - 0.1) - 0.03;
    
    // Anchor shaft
    float shaft = sdBox(p, vec2(0.04, 0.3));
    
    // Crossbar
    float crossbar = sdBox(p - vec2(0.0, -0.1), vec2(0.2, 0.03));
    
    // Flukes (curved bottom arms)
    vec2 pl = p - vec2(-0.15, 0.2);
    vec2 pr = p - vec2(0.15, 0.2);
    float flukeL = sdBox(pl, vec2(0.1, 0.04));
    float flukeR = sdBox(pr, vec2(0.1, 0.04));
    
    return min(min(min(ring, shaft), crossbar), min(flukeL, flukeR));
}

// Fort symbol - fortified structure
float sdFort(vec2 p) {
    // Main wall
    float wall = sdBox(p - vec2(0.0, 0.1), vec2(0.35, 0.15));
    
    // Towers (crenellations)
    float t1 = sdBox(p - vec2(-0.3, -0.1), vec2(0.08, 0.2));
    float t2 = sdBox(p - vec2(0.0, -0.15), vec2(0.08, 0.25));
    float t3 = sdBox(p - vec2(0.3, -0.1), vec2(0.08, 0.2));
    
    // Merlon details
    float m1 = sdBox(p - vec2(-0.3, -0.35), vec2(0.05, 0.05));
    float m2 = sdBox(p - vec2(0.0, -0.45), vec2(0.05, 0.05));
    float m3 = sdBox(p - vec2(0.3, -0.35), vec2(0.05, 0.05));
    
    return min(min(min(wall, min(t1, min(t2, t3))), min(m1, min(m2, m3))), wall);
}

// Temple symbol - columned structure
float sdTemple(vec2 p) {
    // Pediment (triangular roof)
    float roof = sdTriangle(p,
        vec2(-0.35, -0.1),
        vec2(0.0, -0.4),
        vec2(0.35, -0.1));
    
    // Base platform
    float base = sdBox(p - vec2(0.0, 0.3), vec2(0.38, 0.08));
    
    // Columns
    float c1 = sdBox(p - vec2(-0.25, 0.1), vec2(0.04, 0.2));
    float c2 = sdBox(p - vec2(-0.08, 0.1), vec2(0.04, 0.2));
    float c3 = sdBox(p - vec2(0.08, 0.1), vec2(0.04, 0.2));
    float c4 = sdBox(p - vec2(0.25, 0.1), vec2(0.04, 0.2));
    
    return min(min(roof, base), min(min(c1, c2), min(c3, c4)));
}

// Select symbol based on type
float getSymbolSDF(vec2 p, float symbolType) {
    if (symbolType < 0.5) return sdMountain(p);
    if (symbolType < 1.5) return sdCity(p);
    if (symbolType < 2.5) return sdPort(p);
    if (symbolType < 3.5) return sdFort(p);
    return sdTemple(p);
}

void main() {
    vec2 p = v_local;
    
    // Get symbol distance
    float d = getSymbolSDF(p, v_symbol_type);
    
    // Shadow layer
    float shadowAlpha = 0.0;
    if (u_show_shadow) {
        vec2 shadowP = p + vec2(u_shadow_offset, -u_shadow_offset) * 0.05;
        float shadowD = getSymbolSDF(shadowP, v_symbol_type);
        shadowAlpha = smoothstep(0.02, -0.02, shadowD) * u_shadow_color.a;
    }
    
    // Main shape with antialiasing
    float fillAlpha = smoothstep(0.02, -0.02, d);
    
    // Stroke outline
    float strokeD = abs(d) - u_stroke_width * 0.02;
    float strokeAlpha = smoothstep(0.01, -0.01, strokeD);
    
    // Interior (inside stroke)
    float interiorD = d + u_stroke_width * 0.02;
    float interiorAlpha = smoothstep(0.01, -0.01, interiorD);
    
    // Compose final color
    vec4 result = vec4(0.0);
    
    // Apply shadow
    if (shadowAlpha > 0.0) {
        result = mix(result, u_shadow_color, shadowAlpha);
    }
    
    // Apply fill
    if (interiorAlpha > 0.0) {
        result = mix(result, u_fill_color, interiorAlpha * u_fill_color.a);
    }
    
    // Apply stroke
    if (strokeAlpha > 0.0 && interiorAlpha < 0.99) {
        result = mix(result, u_stroke_color, strokeAlpha * u_stroke_color.a);
    }
    
    // Final alpha
    result.a = max(result.a, fillAlpha * max(u_fill_color.a, u_stroke_color.a));
    if (u_show_shadow) {
        result.a = max(result.a, shadowAlpha);
    }
    
    fragColor = result;
}
