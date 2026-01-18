#version 330 core

in vec2 v_uv;
in vec3 v_normal;
in vec3 v_world_pos;
in float v_height;

uniform sampler2D u_base_texture;
uniform sampler2D u_hillshade_texture;
uniform sampler2D u_parchment_texture;

uniform vec3 u_light_direction;
uniform float u_ambient_strength;
uniform float u_hillshade_strength;
uniform float u_ao_strength;

uniform bool u_use_hillshade;
uniform bool u_use_parchment;
uniform bool u_use_lighting;

uniform vec3 u_water_deep_color;
uniform vec3 u_water_shallow_color;
uniform vec3 u_lowland_tint;
uniform vec3 u_highland_tint;
uniform vec3 u_mountain_tint;

uniform float u_elevation_scale;

out vec4 fragColor;

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

float computeAO(vec3 normal) {
  // Enhanced ambient occlusion based on slope and height
  float ao = clamp(normal.y * 0.5 + 0.5, 0.0, 1.0);
  // Add extra darkening in valleys based on slope steepness
  float slope = 1.0 - abs(normal.y);
  ao *= 1.0 - slope * 0.3;
  return mix(1.0, ao, u_ao_strength);
}

float computeHillshade(vec3 normal, vec3 lightDir) {
  float ndotl = max(0.0, dot(normal, lightDir));
  // Enhanced hillshade with rim lighting for depth
  float rimLight = pow(1.0 - abs(dot(normal, vec3(0.0, 1.0, 0.0))), 2.0) * 0.15;
  float shade = u_ambient_strength + (1.0 - u_ambient_strength) * ndotl;
  return shade + rimLight;
}

vec3 getElevationTint(float height) {
  if (height < 0.0) {
    // Water - deeper contrast
    float depth = clamp(-height, 0.0, 1.0);
    depth = pow(depth, 0.8); // Non-linear depth curve for visual interest
    return mix(u_water_shallow_color, u_water_deep_color, depth);
  } else if (height < 0.15) {
    // Coastal lowlands - warmer tones
    float t = height / 0.15;
    vec3 coastTint = u_lowland_tint * vec3(1.02, 1.0, 0.96);
    return mix(coastTint, u_lowland_tint, t);
  } else if (height < 0.45) {
    // Lowlands to highlands transition
    float t = (height - 0.15) / 0.30;
    return mix(u_lowland_tint, u_highland_tint, t);
  } else if (height < 0.75) {
    // Highlands
    float t = (height - 0.45) / 0.30;
    return mix(u_highland_tint, u_mountain_tint, t);
  } else {
    // High mountains - cooler tones with snow hints
    float t = clamp((height - 0.75) / 0.25, 0.0, 1.0);
    vec3 snowTint = u_mountain_tint * vec3(1.1, 1.12, 1.15);
    return mix(u_mountain_tint, snowTint, t * 0.5);
  }
}

float getParchmentPattern(vec2 uv) {

  float n1 = fbm(uv * 8.0, 3);
  float n2 = fbm(uv * 20.0 + vec2(100.0), 2);

  float pattern = n1 * 0.6 + n2 * 0.4;

  return 0.88 + pattern * 0.12;
}

void main() {

  vec2 uv = vec2(v_uv.x, 1.0 - v_uv.y);
  vec4 baseColor = texture(u_base_texture, uv);

  vec3 color = baseColor.rgb;

  // Apply elevation-based coloring with enhanced contrast
  vec3 elevationTint = getElevationTint(v_height * u_elevation_scale);
  color *= elevationTint;

  // Pre-compute slope for reuse
  float slope = 1.0 - abs(v_normal.y);

  // Enhanced hillshading for dramatic 3D effect
  if (u_use_hillshade) {
    float hillshade = computeHillshade(v_normal, normalize(u_light_direction));
    // Apply hillshade with enhanced contrast
    color *= mix(1.0, hillshade, u_hillshade_strength);
    
    // Add subtle height-based brightness for peaks
    if (v_height > 0.5) {
      float peakBrightness = (v_height - 0.5) * 0.1;
      color += vec3(peakBrightness);
    }
  }

  if (u_use_lighting) {
    vec3 lightDir = normalize(u_light_direction);
    float diffuse = max(0.0, dot(v_normal, lightDir));
    
    // Add half-lambert for softer shadow transitions
    float halfLambert = diffuse * 0.5 + 0.5;
    halfLambert = halfLambert * halfLambert;
    
    float lighting = u_ambient_strength + (1.0 - u_ambient_strength) * mix(diffuse, halfLambert, 0.4);

    float ao = computeAO(v_normal);
    
    // Apply lighting with height-based adjustment
    float heightFactor = clamp(v_height * 0.3 + 0.85, 0.7, 1.0);
    color *= lighting * ao * heightFactor;
  }

  if (u_use_parchment) {
    float parchment = getParchmentPattern(v_uv);
    color *= parchment;
    color = mix(color, color * vec3(1.02, 1.0, 0.96), 0.3);
  }

  // Height-based fog/atmosphere for depth perception
  float atmosphereStrength = clamp((1.0 - v_height) * 0.12, 0.0, 0.15);
  vec3 fogColor = vec3(0.75, 0.78, 0.82);
  color = mix(color, fogColor, atmosphereStrength);

  // Reuse pre-computed slope for edge darkening
  color *= 1.0 - slope * 0.15;

  // Vignette
  vec2 vignetteCoord = v_uv * 2.0 - 1.0;
  float vignette = 1.0 - dot(vignetteCoord, vignetteCoord) * 0.15;
  color *= vignette;

  fragColor = vec4(color, baseColor.a);
}
