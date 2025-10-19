#version 330 core
out vec4 FragColor;
in vec2 TexCoord;
in vec3 WorldPos;

uniform float time;

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
const float PI = 3.14159265359;

mat2 rot(float a){ float c=cos(a), s=sin(a); return mat2(c,-s,s,c); }

// Hash / noise / fbm (kept compatible with original)
float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
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

float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    float f = 1.0;
    for(int i=0; i<5; i++){
        v += a * noise(p * f);
        f *= 2.0;
        a *= 0.5;
    }
    return v;
}

// Lightweight Voronoi for animated caustics
float voronoi(vec2 p) {
    vec2 n = floor(p);
    vec2 f = fract(p);
    float md = 1.0;
    for(int j=-1; j<=1; j++){
        for(int i=-1; i<=1; i++){
            vec2 g = vec2(float(i), float(j));
            vec2 r = vec2(hash(n+g), hash(n+g+1.37));
            r = 0.5 + 0.5 * sin(time*0.5 + 6.2831*r);
            vec2 d = g + r - f;
            md = min(md, length(d));
        }
    }
    return md;
}

// ------------------------------------------------------------
// Water surface synthesis (height field only for shading)
// ------------------------------------------------------------

// Domain warping to break tiling and add turbulence
vec2 warp(vec2 uv){
    vec2 w = vec2(fbm(uv*0.8 + time*0.05),
                  fbm(uv*0.9 - time*0.04));
    return uv + 0.25*w;
}

// Multi-directional sine/gerstner-ish waves (height only)
float waveHeight(vec2 uv){
    // Three primary swell directions
    vec2 d1 = normalize(vec2(1.0, 0.3));
    vec2 d2 = normalize(vec2(-0.6, 1.0));
    vec2 d3 = normalize(vec2(0.2, 1.0));

    uv = warp(uv);

    float h  = 0.0;
    float t  = time;

    // Long swells
    h += 0.35 * sin(dot(uv*1.6, d1)*6.0 - t*0.8);
    h += 0.25 * sin(dot(uv*1.9, d2)*7.0 - t*0.9);
    // Choppy mid-frequency
    h += 0.20 * sin(dot(uv*3.2, d3)*10.5 - t*1.6);
    // Fine ripples via fbm
    h += 0.10 * (fbm(uv*4.0 + t*0.2) - 0.5);
    h += 0.05 * (fbm(rot(1.2)*uv*7.0 - t*0.35) - 0.5);

    return h;
}

// Estimate normal from screen-space derivatives of the height field
vec3 waterNormal(float h){
    // Derivatives of height across screen; scale to control "normal strength"
    float sx = dFdx(h);
    float sy = dFdy(h);
    float strength = 8.0; // increase for choppier normals
    vec3 N = normalize(vec3(-sx*strength, 1.0, -sy*strength));
    return N;
}

// Simple sky model for reflections
vec3 skyColor(vec3 rd, vec3 sunDir){
    float t = clamp(rd.y*0.5 + 0.5, 0.0, 1.0);
    vec3 horizon = vec3(0.75, 0.85, 0.95);
    vec3 zenith  = vec3(0.20, 0.42, 0.70);
    vec3 sky = mix(horizon, zenith, t);
    // Sun glow
    float sun = pow(max(dot(rd, sunDir), 0.0), 600.0);
    sky += vec3(1.0, 0.95, 0.85) * sun * 2.5;
    return sky;
}

// Schlick Fresnel
float fresnelSchlick(float cosTheta, float F0){
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// Minimal GGX specular (Smith-Schlick) for a crisp sun highlight
float ggxSpecular(vec3 N, vec3 V, vec3 L, float rough, float F0){
    vec3 H = normalize(V + L);
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    float a = max(rough*rough, 0.001);
    float a2 = a*a;

    float denom = (NdotH*NdotH*(a2-1.0)+1.0);
    float D = a2 / (PI * denom * denom);

    float k = (a + 1.0);
    k = (k*k)/8.0;
    float Gv = NdotV / (NdotV*(1.0 - k) + k);
    float Gl = NdotL / (NdotL*(1.0 - k) + k);
    float G = Gv * Gl;

    float F = fresnelSchlick(VdotH, F0);
    return (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------
void main()
{
    // Use a mix of TexCoord and WorldPos to reduce tiling and anchor to world
    vec2 uv = TexCoord * 4.0 + WorldPos.xz * 0.15;

    // Height field and derived features
    float h = waveHeight(uv);
    vec3  N = waterNormal(h);

    // Lighting setup (constants: no new uniforms)
    vec3 sunDir = normalize(vec3(0.28, 0.85, 0.43)); // warm afternoon sun
    vec3 V      = normalize(vec3(0.0, 0.7, 0.7));    // approximate view from above
    // (If you prefer a fixed camera anchor, uncomment this instead)
    // vec3 camPos = vec3(0.0, 2.7, 3.0);
    // vec3 V = normalize(camPos - WorldPos);

    // Fresnel & reflection
    float NdotV = max(dot(N, V), 0.0);
    float F0 = 0.02; // water IOR ~1.33
    float F = fresnelSchlick(NdotV, F0);

    vec3 R = reflect(-V, N);
    vec3 reflection = skyColor(R, sunDir);

    // Base transmission color (absorption-tinted)
    vec3 deepWater    = vec3(0.02, 0.07, 0.11);
    vec3 shallowWater = vec3(0.12, 0.30, 0.38);

    // Pseudo "shallowness": calmer patches look shallower; also tie to uv to vary
    float calm = smoothstep(0.0, 0.45, abs(h));
    float shallowFactor = clamp(0.35 + 0.35*(fbm(uv*0.6) * (1.0 - calm)), 0.0, 1.0);
    vec3 transmission = mix(deepWater, shallowWater, shallowFactor);

    // Caustics: brighten transmission where cells converge
    float c1 = voronoi(uv*2.0 + time*0.1);
    float c2 = voronoi(rot(0.7)*(uv*1.5 - time*0.08));
    float caustics = pow(1.0 - 0.7*(c1*0.6 + c2*0.4), 2.5);
    transmission += vec3(0.55, 0.70, 0.85) * caustics * 0.20;

    // Sun lighting (GGX specular + a touch of diffuse subsurface)
    float roughness = mix(0.08, 0.18, smoothstep(0.0, 0.6, length(vec2(dFdx(h), dFdy(h)))));
    float spec = ggxSpecular(N, V, sunDir, roughness, F0);
    float NdotL = max(dot(N, sunDir), 0.0);
    vec3 sunDiffuse = transmission * NdotL * 0.25;

    // Foam: crest (steepness) + screen edge foam from original idea
    float steep = length(vec2(dFdx(h), dFdy(h)));
    float crestFoam = smoothstep(0.38, 0.95, steep);
    // Animate foam breakup
    crestFoam *= 0.6 + 0.4*fbm(uv*3.5 + time*0.7);

    // Edge foam (reuse TexCoord idea but subtler and 2D)
    float edgeX = smoothstep(0.02, 0.12, TexCoord.x) * smoothstep(0.02, 0.12, 1.0 - TexCoord.x);
    float edgeY = smoothstep(0.02, 0.12, TexCoord.y) * smoothstep(0.02, 0.12, 1.0 - TexCoord.y);
    float frameEdge = (1.0 - edgeX*edgeY);
    float foamNoise = noise(uv*6.0 + time*0.5);
    float foam = clamp(crestFoam*0.85 + frameEdge*foamNoise*0.35, 0.0, 1.0);

    vec3 foamColor = vec3(0.93, 0.96, 1.0);

    // Combine transmission and reflection with Fresnel
    vec3 color = mix(transmission, reflection, F);
    // Add lighting
    color += vec3(1.0) * spec * 1.2;
    color += sunDiffuse;

    // Mix in foam on top
    color = mix(color, foamColor, foam);

    // Subtle blue highlight along glancing angles
    color += vec3(0.05, 0.08, 0.12) * pow(1.0 - NdotV, 3.0);

    // Final tone and alpha
    FragColor = vec4(color, 0.85);
}
