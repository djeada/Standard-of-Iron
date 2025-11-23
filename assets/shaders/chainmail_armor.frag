#version 330 core

in vec3 v_normal;
in vec3 v_worldPos;
in vec3 v_viewDir;
in vec2 v_texCoord;
in float v_ringPhase;

uniform vec3 u_color;
uniform float u_alpha;
uniform vec3 u_lightDir;
uniform vec3 u_lightColor;
uniform vec3 u_ambientColor;
uniform float u_rustAmount; // 0.0 = pristine, 1.0 = heavily rusted
uniform int u_materialId;

out vec4 FragColor;

// Hash function for procedural noise
float hash(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

// 2D noise
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

// Chainmail ring pattern - interlocking rings
float chainmailRingPattern(vec2 uv, float phase) {
  vec2 uv_scaled = uv * 64.0; // Ring density

  // Apply row offset for interlocking
  uv_scaled.x += phase * 0.5;

  vec2 grid = fract(uv_scaled) - 0.5;
  float ring_dist = length(grid);

  // Outer ring
  float outer_ring =
      smoothstep(0.45, 0.40, ring_dist) - smoothstep(0.35, 0.30, ring_dist);

  // Inner ring (creates depth)
  float inner_ring =
      smoothstep(0.32, 0.28, ring_dist) - smoothstep(0.25, 0.20, ring_dist);

  // Combine rings
  float ring = outer_ring + inner_ring * 0.5;

  // Add ring overlap highlights (where rings interlock)
  vec2 overlap_grid = fract(uv_scaled + vec2(0.5, 0.0)) - 0.5;
  float overlap_dist = length(overlap_grid);
  float overlap = smoothstep(0.25, 0.22, overlap_dist) * 0.3;

  return clamp(ring + overlap, 0.0, 1.0);
}

// Rust and weathering
vec3 applyRust(vec3 baseColor, vec3 worldPos, float rustAmount) {
  // Rust color palette
  vec3 rust_dark = vec3(0.35, 0.15, 0.10);   // Dark rust/brown
  vec3 rust_bright = vec3(0.65, 0.35, 0.20); // Orange rust
  vec3 rust_green = vec3(0.25, 0.40, 0.35);  // Green oxidation

  // Procedural rust distribution
  float rust_noise1 = noise(worldPos.xz * 20.0);
  float rust_noise2 = noise(worldPos.xy * 15.0);
  float rust_noise3 = noise(worldPos.yz * 18.0);

  float combined_noise = (rust_noise1 + rust_noise2 + rust_noise3) / 3.0;

  // More rust in lower areas (gravity/water accumulation)
  float height_factor = clamp(1.0 - worldPos.y * 0.6, 0.0, 1.0);

  // Total rust amount
  float total_rust = combined_noise * rustAmount * height_factor;
  total_rust = clamp(total_rust, 0.0, 1.0);

  // Mix rust colors
  vec3 rust_mix = mix(rust_dark, rust_bright, rust_noise1);
  rust_mix = mix(rust_mix, rust_green, rust_noise2 * 0.3);

  return mix(baseColor, rust_mix, total_rust);
}

// Ring highlights and shadows
float ringAO(vec2 uv, float phase) {
  vec2 uv_scaled = uv * 64.0;
  uv_scaled.x += phase * 0.5;

  vec2 grid = fract(uv_scaled) - 0.5;
  float dist = length(grid);

  // Ambient occlusion in ring valleys
  float ao = smoothstep(0.15, 0.35, dist);
  return ao;
}

// Fresnel for metal reflection
float fresnel(vec3 viewDir, vec3 normal, float power) {
  return pow(1.0 - max(dot(viewDir, normal), 0.0), power);
}

void main() {
  vec3 N = normalize(v_normal);
  vec3 V = normalize(v_viewDir);
  vec3 L = normalize(u_lightDir);
  vec3 H = normalize(V + L);

  // Brighter steel base
  vec3 baseColor = u_color * 1.2;

  // Subtle chainmail pattern
  float ringPattern = chainmailRingPattern(v_texCoord, v_ringPhase);
  vec3 ringColor = mix(baseColor * 0.85, baseColor * 1.15, ringPattern * 0.6);

  // Light rust only
  ringColor = applyRust(ringColor, v_worldPos, u_rustAmount * 0.5);

  // Strong metallic lighting
  float NdotL = max(dot(N, L), 0.0);
  float NdotH = max(dot(N, H), 0.0);
  float NdotV = max(dot(N, V), 0.0);

  // Bright diffuse
  vec3 diffuse = ringColor * NdotL * 1.5;

  // Strong specular
  float spec_power = 64.0;
  float specular = pow(NdotH, spec_power) * 1.8;

  // Bright ambient
  vec3 ambient = u_ambientColor * ringColor * 0.6;

  // Fresnel rim
  float rim = pow(1.0 - NdotV, 3.0) * 0.5;

  // Combine with boosted brightness
  vec3 color = ambient + (diffuse + vec3(specular)) * u_lightColor;
  color += vec3(rim) * u_lightColor;

  // Add sparkle from rings
  float sparkle = ringPattern * NdotH * 0.4;
  color += vec3(sparkle);

  // Brighten overall
  color *= 1.4;

  // Gentle tone mapping
  color = color / (color + vec3(0.5));

  // Gamma
  color = pow(color, vec3(1.0 / 2.2));

  FragColor = vec4(color, u_alpha);
}
