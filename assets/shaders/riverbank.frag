#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 WorldPos;
in vec3 Normal;

uniform float time;

// Noise functions for texture variation
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
  float b = hash(i + vec2(1, 0));
  float c = hash(i + vec2(0, 1));
  float d = hash(i + vec2(1, 1));
  
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
  float value = 0.0;
  float amplitude = 0.5;
  float frequency = 1.0;
  
  for (int i = 0; i < 4; i++) {
    value += amplitude * noise(p * frequency);
    frequency *= 2.0;
    amplitude *= 0.5;
  }
  
  return value;
}

void main() {
  // UV coordinates for noise patterns
  vec2 uv = WorldPos.xz * 0.5;
  
  // Wet/muddy colors near water edge
  vec3 wetSoil = vec3(0.18, 0.15, 0.12);     // Dark, wet mud
  vec3 dampSoil = vec3(0.35, 0.28, 0.22);    // Damp soil
  vec3 drySoil = vec3(0.48, 0.42, 0.35);     // Dry sand/soil
  vec3 grassTint = vec3(0.38, 0.48, 0.30);   // Grass mixed with soil
  
  // Transition from wet to dry based on TexCoord.x
  // 0.0 = water edge (wet), 1.0 = land edge (dry)
  float wetness = 1.0 - TexCoord.x;
  
  // Add procedural variation
  float variation = fbm(uv * 3.0 + time * 0.1);
  float detailNoise = noise(uv * 15.0);
  
  // Blend colors based on wetness and variation
  vec3 baseColor;
  if (wetness > 0.7) {
    // Very wet zone - dark mud with subtle shine
    baseColor = mix(wetSoil, dampSoil, variation);
  } else if (wetness > 0.4) {
    // Damp transition zone
    baseColor = mix(dampSoil, drySoil, (0.7 - wetness) / 0.3);
    baseColor = mix(baseColor, grassTint, variation * 0.3);
  } else {
    // Dry zone blending to grass
    baseColor = mix(drySoil, grassTint, wetness / 0.4 + variation * 0.2);
  }
  
  // Add detail variation (small rocks, soil patches)
  baseColor *= (0.85 + detailNoise * 0.3);
  
  // Simple lighting
  vec3 lightDir = normalize(vec3(0.3, 0.8, 0.4));
  float diffuse = max(dot(Normal, lightDir), 0.0);
  vec3 ambient = vec3(0.4, 0.4, 0.45);
  
  vec3 lighting = ambient + diffuse * vec3(0.6, 0.6, 0.55);
  vec3 color = baseColor * lighting;
  
  // Add subtle wetness shine on wet areas
  if (wetness > 0.5) {
    vec3 viewDir = normalize(vec3(0.0, 1.0, 0.5));
    vec3 reflectDir = reflect(-lightDir, Normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 8.0);
    color += spec * wetness * vec3(0.15, 0.15, 0.12);
  }
  
  FragColor = vec4(color, 1.0);
}
