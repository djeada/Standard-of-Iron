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
  
  // Base bronze color with procedural weathering
  vec3 baseColor = bronzePatina(u_color, v_worldPos);
  
  // Add hammered texture detail
  float hammer = hammeredTexture(v_texCoord);
  baseColor = mix(baseColor, baseColor * 0.85, hammer);
  
  // Add rivet highlights
  float rivets = rivetDetail(v_texCoord);
  baseColor = mix(baseColor, baseColor * 1.3, rivets);
  
  // PBR lighting calculation
  vec3 F0 = mix(vec3(0.04), baseColor, v_metallic);
  vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
  
  // Specular component
  float NDF = distributionGGX(N, H, v_roughness);
  float NdotL = max(dot(N, L), 0.0);
  float NdotV = max(dot(N, V), 0.0);
  
  vec3 specular = NDF * F / max(4.0 * NdotV * NdotL, 0.001);
  
  // Diffuse component
  vec3 kD = (vec3(1.0) - F) * (1.0 - v_metallic);
  vec3 diffuse = kD * baseColor / 3.14159265;
  
  // Combine lighting
  vec3 ambient = u_ambientColor * baseColor * 0.3;
  vec3 radiance = u_lightColor * NdotL;
  vec3 color = ambient + (diffuse + specular) * radiance;
  
  // Edge wear (brighter on edges from polishing)
  float edge_factor = pow(1.0 - NdotV, 3.0);
  color = mix(color, color * 1.4, edge_factor * 0.3);
  
  // Tone mapping
  color = color / (color + vec3(1.0));
  
  // Gamma correction
  color = pow(color, vec3(1.0 / 2.2));
  
  FragColor = vec4(color, u_alpha);
}
