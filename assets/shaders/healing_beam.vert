#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;

uniform mat4 u_mvp;
uniform mat4 u_model;
uniform float u_time;
uniform float u_progress;      // 0.0 = start, 1.0 = complete
uniform vec3 u_startPos;       // Healer position (world space)
uniform vec3 u_endPos;         // Target position (world space)
uniform float u_beamWidth;     // Base beam width

out vec3 v_worldPos;
out vec3 v_normal;
out vec2 v_texCoord;
out float v_beamT;             // Position along beam (0-1)
out float v_edgeDist;          // Distance from beam center
out float v_glowIntensity;

// Smooth curve for beam arc
vec3 bezierArc(float t, vec3 start, vec3 end, float arcHeight) {
    vec3 mid = (start + end) * 0.5;
    mid.y += arcHeight;
    
    // Quadratic bezier
    vec3 p0 = mix(start, mid, t);
    vec3 p1 = mix(mid, end, t);
    return mix(p0, p1, t);
}

// Get tangent direction along beam
vec3 bezierTangent(float t, vec3 start, vec3 end, float arcHeight) {
    vec3 mid = (start + end) * 0.5;
    mid.y += arcHeight;
    
    vec3 d0 = mid - start;
    vec3 d1 = end - mid;
    return normalize(mix(d0, d1, t));
}

void main() {
    // a_position.z is the position along the beam (0-1)
    // a_position.x, a_position.y define the cross-section
    float t = a_position.z;
    v_beamT = t;
    
    // Only render up to current progress (beam grows over time)
    float visibleT = min(t, u_progress * 1.2);
    
    // Calculate arc height based on distance
    float dist = length(u_endPos - u_startPos);
    float arcHeight = dist * 0.25;
    
    // Get position along beam curve
    vec3 beamCenter = bezierArc(visibleT, u_startPos, u_endPos, arcHeight);
    vec3 tangent = bezierTangent(visibleT, u_startPos, u_endPos, arcHeight);
    
    // Build local coordinate frame
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(tangent, up));
    vec3 localUp = normalize(cross(right, tangent));
    
    // Spiral twist along beam for magical effect
    float spiralAngle = t * 12.56637 + u_time * 4.0; // 2 full rotations + animation
    float spiralRadius = u_beamWidth * (0.3 + 0.7 * sin(t * 3.14159)); // Barrel shape
    
    // Apply spiral offset
    vec3 spiralOffset = right * cos(spiralAngle) * spiralRadius 
                      + localUp * sin(spiralAngle) * spiralRadius;
    
    // Cross-section position
    float sectionRadius = u_beamWidth * (1.0 - abs(a_position.x));
    vec3 sectionOffset = right * a_position.x * sectionRadius 
                       + localUp * a_position.y * sectionRadius;
    
    // Combine all offsets
    vec3 worldPos = beamCenter + sectionOffset * 0.3 + spiralOffset * 0.7;
    
    // Add flowing wave effect
    float wave = sin(t * 20.0 - u_time * 8.0) * 0.02 * u_beamWidth;
    worldPos += localUp * wave;
    
    v_worldPos = worldPos;
    v_edgeDist = length(vec2(a_position.x, a_position.y));
    
    // Glow intensity: brighter at center, pulsing over time
    float pulse = 0.7 + 0.3 * sin(u_time * 6.0 + t * 10.0);
    v_glowIntensity = (1.0 - v_edgeDist) * pulse;
    
    // Fade out beyond progress point
    if (t > u_progress * 1.1) {
        v_glowIntensity *= 0.0;
    }
    
    // Transform normal
    v_normal = mat3(u_model) * a_normal;
    v_texCoord = vec2(t, a_position.x * 0.5 + 0.5);
    
    gl_Position = u_mvp * vec4(worldPos, 1.0);
}
