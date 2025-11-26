#version 330 core

in vec3 v_worldPos;
in vec3 v_normal;
in vec2 v_texCoord;
in float v_height;

uniform vec3 u_color;
uniform int u_materialId; // 0: Skin, 1: Armor, 2: Helmet, 3: Weapon, 4: Shield
uniform float u_time;

out vec4 FragColor;

// --- Constants & Config ---
const float PI = 3.14159265359;
const vec3 LIGHT_DIR = normalize(vec3(0.5, 1.0, 0.5));
const vec3 CARTHAGE_PURPLE = vec3(0.45, 0.05, 0.35); // Tyrian Purple
const vec3 BRONZE_ALBEDO = vec3(0.85, 0.65, 0.45);
const vec3 IRON_ALBEDO = vec3(0.56, 0.57, 0.58);
const vec3 SKIN_ALBEDO = vec3(0.85, 0.65, 0.55);
const vec3 LINEN_ALBEDO = vec3(0.92, 0.90, 0.85);
const vec3 WOOD_ALBEDO = vec3(0.40, 0.25, 0.15);

// --- Noise Functions ---
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i + vec2(0.0, 0.0)), hash(i + vec2(1.0, 0.0)), u.x),
               mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x), u.y);
}

float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 5; i++) {
        v += a * noise(p);
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}

// --- PBR Functions ---
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / max(denom, 0.00001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return num / max(denom, 0.00001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// --- Material Logic ---

struct Material {
    vec3 albedo;
    float roughness;
    float metallic;
    float ao;
    vec3 normal;
};

Material getSkinMaterial(vec3 pos, vec3 N) {
    Material m;
    m.albedo = SKIN_ALBEDO;
    // Add some variation
    float n = fbm(pos.xy * 10.0);
    m.albedo = mix(m.albedo, m.albedo * 0.9, n * 0.2);
    m.roughness = 0.6 + n * 0.1;
    m.metallic = 0.0;
    m.ao = 1.0;
    m.normal = N;
    return m;
}

Material getHelmetMaterial(vec3 pos, vec3 N) {
    Material m;
    // Bronze base
    m.albedo = BRONZE_ALBEDO;
    m.metallic = 1.0;
    m.roughness = 0.35;
    
    // Hammered noise
    float hammer = fbm(pos.xz * 20.0);
    m.roughness += hammer * 0.15;
    m.normal = normalize(N + vec3(hammer * 0.1)); // Fake normal perturbation

    // Purple Crest / Detail
    // Assume crest is at top of helmet (high Y)
    if (v_height > 1.6) { // Approximate height check
        m.albedo = CARTHAGE_PURPLE;
        m.metallic = 0.1; // Lacquered/Dyed
        m.roughness = 0.6;
    }
    
    m.ao = 1.0;
    return m;
}

Material getArmorMaterial(vec3 pos, vec3 N) {
    Material m;
    
    // Linothorax (Linen) vs Bronze Scales
    // Simple pattern: Scales on chest, Linen elsewhere
    float scalePattern = sin(pos.x * 40.0) * sin(pos.y * 40.0);
    bool isScale = (pos.y > 1.0 && pos.y < 1.5 && scalePattern > 0.0);

    if (isScale) {
        m.albedo = BRONZE_ALBEDO;
        m.metallic = 1.0;
        m.roughness = 0.4;
        // Scale normals
        m.normal = normalize(N + vec3(0.0, -0.2, 0.0)); // Tilt scales down
    } else {
        // Linen
        m.albedo = LINEN_ALBEDO;
        // Weave noise
        float weave = sin(pos.x * 200.0) * sin(pos.y * 200.0);
        m.albedo *= (0.9 + weave * 0.1);
        m.metallic = 0.0;
        m.roughness = 0.8;
        m.normal = N;
        
        // Purple trim
        if (pos.y < 0.9) { // Pteruges (skirt strips)
             m.albedo = CARTHAGE_PURPLE;
        }
    }
    
    m.ao = 1.0;
    return m;
}

Material getWeaponMaterial(vec3 pos, vec3 N) {
    Material m;
    // Spear: Wood shaft, Metal tip
    if (pos.y > 1.8) { // Tip
        m.albedo = IRON_ALBEDO;
        m.metallic = 1.0;
        m.roughness = 0.3;
    } else { // Shaft
        m.albedo = WOOD_ALBEDO;
        // Wood grain
        float grain = fbm(pos.xy * vec2(5.0, 50.0));
        m.albedo *= (0.8 + grain * 0.4);
        m.metallic = 0.0;
        m.roughness = 0.7;
    }
    m.normal = N;
    m.ao = 1.0;
    return m;
}

void main() {
    vec3 N = normalize(v_normal);
    vec3 V = normalize(vec3(0.0, 0.0, 1.0)); // Simplified view vector (Headlamp)
    
    Material mat;
    
    // Region Selection based on Material ID
    if (u_materialId == 2) {
        mat = getHelmetMaterial(v_worldPos, N);
    } else if (u_materialId == 1) {
        mat = getArmorMaterial(v_worldPos, N);
    } else if (u_materialId == 3) {
        mat = getWeaponMaterial(v_worldPos, N);
    } else {
        mat = getSkinMaterial(v_worldPos, N);
    }

    // Lighting Calculation
    vec3 L = LIGHT_DIR;
    vec3 H = normalize(V + L);
    
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, mat.albedo, mat.metallic);

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(mat.normal, H, mat.roughness);   
    float G   = GeometrySmith(mat.normal, V, L, mat.roughness);      
    vec3 F    = FresnelSchlick(max(dot(H, V), 0.0), F0);
       
    vec3 numerator    = NDF * G * F; 
    float denominator = 4.0 * max(dot(mat.normal, V), 0.0) * max(dot(mat.normal, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - mat.metallic;	  

    float NdotL = max(dot(mat.normal, L), 0.0);        

    vec3 Lo = (kD * mat.albedo / PI + specular) * vec3(1.0) * NdotL; 
    
    // Ambient
    vec3 ambient = vec3(0.03) * mat.albedo * mat.ao;
    
    vec3 color = ambient + Lo;

    // Tone mapping (Reinhard)
    color = color / (color + vec3(1.0));
    // Gamma correction
    color = pow(color, vec3(1.0/2.2)); 

    FragColor = vec4(color, 1.0);
}
