#version 330 core

in vec3 v_normal;
in vec3 v_worldPos;
in vec3 v_viewDir;
in vec2 v_texCoord;
in float v_metallic;
in float v_roughness;

uniform vec3 u_color;
uniform float u_alpha;
uniform vec3 u_lightDir; // Sun direction
uniform vec3 u_lightColor;
uniform vec3 u_ambientColor;

out vec4 FragColor;

// Fresnel-Schlick approximation for metallic reflection
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
  return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// GGX normal distribution for specular highlights
float distributionGGX(vec3 N, vec3 H, float roughness) {
  float a = roughness * roughness;
  float a2 = a * a;
  float NdotH = max(dot(N, H), 0.0);
  float NdotH2 = NdotH * NdotH;
  
  float denom = (NdotH2 * (a2 - 1.0) + 1.0);
  denom = 3.14159265 * denom * denom;
  
  return a2 / max(denom, 0.0001);
}

// Procedural bronze oxidation/patina
vec3 bronzePatina(vec3 baseColor, vec3 worldPos) {
  // Green-blue patina in crevices and lower areas
  float patina_noise = fract(sin(dot(worldPos.xz, vec2(12.9898, 78.233))) * 43758.5453);
  float patina_amount = smoothstep(1.5, 1.7, worldPos.y) * 0.3 * patina_noise;
  
  vec3 patina_color = vec3(0.2, 0.55, 0.45); // Verdigris green
  return mix(baseColor, patina_color, patina_amount);
}

// Hammered metal texture
float hammeredTexture(vec2 uv) {
  vec2 id = floor(uv * 32.0);
  float n = fract(sin(dot(id, vec2(12.9898, 78.233))) * 43758.5453);
  
  vec2 local = fract(uv * 32.0) - 0.5;
  float dist = length(local);
  
  // Circular hammer marks
  float hammer = smoothstep(0.4, 0.3, dist) * (0.5 + n * 0.5);
  return hammer * 0.15;
}

// Rivet detail
float rivetDetail(vec2 uv) {
  vec2 rivet_grid = fract(uv * 6.0) - 0.5;
  float rivet_dist = length(rivet_grid);
  
  float rivet = smoothstep(0.08, 0.05, rivet_dist);
  rivet *= smoothstep(0.12, 0.10, rivet_dist); // Ring around rivet
  
  return rivet * 0.25;
}

void main() {
  vec3 N = normalize(v_normal);
  vec3 V = normalize(v_viewDir);
  vec3 L = normalize(u_lightDir);
  vec3 H = normalize(V + L);
  
  // High-quality bronze material
  vec3 baseColor = u_color;
  
  // Subtle patina only in deep recesses
  float ambient_factor = max(dot(N, vec3(0.0, 1.0, 0.0)), 0.0);
  vec3 patina_color = vec3(0.25, 0.45, 0.35);
  baseColor = mix(baseColor, patina_color, (1.0 - ambient_factor) * 0.12);
  
  // Advanced PBR
  vec3 F0 = vec3(0.95, 0.64, 0.54); // Bronze F0
  vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
  
  float NDF = distributionGGX(N, H, v_roughness);
  float NdotL = max(dot(N, L), 0.0);
  float NdotV = max(dot(N, V), 0.0);
  
  // Cook-Torrance specular
  float G = min(1.0, min(2.0 * NdotV * NdotL / max(dot(V, H), 0.001),
                         2.0 * NdotV * NdotL / max(dot(V, H), 0.001)));
  
  vec3 specular = (NDF * F * G) / max(4.0 * NdotV * NdotL, 0.001);
  specular = clamp(specular, 0.0, 10.0);
  
  // Metallic doesn't have diffuse
  vec3 kD = vec3(0.0);
  vec3 diffuse = kD * baseColor / 3.14159265;
  
  // Strong ambient for visibility
  vec3 ambient = u_ambientColor * baseColor * 0.5;
  
  // Main lighting
  vec3 radiance = u_lightColor * NdotL * 1.8;
  vec3 color = ambient + (diffuse + specular * 2.5) * radiance;
  
  // Strong rim light for definition
  float rim = pow(1.0 - NdotV, 2.5) * NdotL;
  color += baseColor * rim * 0.6;
  
  // Brighten overall
  color *= 1.3;
  
  // Gentle tone mapping
  color = color / (color + vec3(0.5));
  
  // Gamma
  color = pow(color, vec3(1.0 / 2.2));
  
  FragColor = vec4(color, u_alpha);
}
