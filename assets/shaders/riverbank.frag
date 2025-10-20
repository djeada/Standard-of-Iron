#version 330 core
out vec4 FragColor;

in vec2 TexCoord;   // x = across river (0 water edge -> 1 land edge)
in vec3 WorldPos;
in vec3 Normal;

uniform float time;

// ------------------------- utils -------------------------
float saturate(float x){ return clamp(x, 0.0, 1.0); }
vec2  saturate(vec2 v){ return clamp(v, vec2(0.0), vec2(1.0)); }
vec3  saturate(vec3 v){ return clamp(v, vec3(0.0), vec3(1.0)); }

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
  float v = 0.0;
  float a = 0.5;
  float f = 1.0;
  for (int i = 0; i < 5; i++) {
    v += a * noise(p * f);
    f *= 2.0;
    a *= 0.5;
  }
  return v;
}

// Slight domain warp to break patterns
vec2 warp(vec2 uv) {
  vec2 w1 = vec2(fbm(uv * 0.7 + time * 0.06), fbm(uv * 0.7 - time * 0.05));
  vec2 w2 = vec2(fbm(uv * 1.3 - time * 0.03), fbm(uv * 0.9 + time * 0.04));
  return uv + 0.25 * w1 + 0.15 * w2;
}

// ------------------------- main -------------------------
void main() {
  // World-anchored UVs (no per-tile repeats)
  vec2 uv = warp(WorldPos.xz * 0.45);

  // Base palettes
  vec3 wetSoil   = vec3(0.16, 0.13, 0.11); // very wet, near waterline
  vec3 dampSoil  = vec3(0.30, 0.24, 0.20);
  vec3 drySoil   = vec3(0.48, 0.41, 0.33);
  vec3 grassTint = vec3(0.36, 0.48, 0.30);

  // --- Wetness ---
  // Shore width from TexCoord.x (0 at water, 1 at land), with irregular jitter
  float shore    = 1.0 - TexCoord.x;
  float jitter   = (fbm(uv * 3.0) - 0.5) * 0.35;      // noisy shoreline edge
  float slope    = 1.0 - saturate(Normal.y);          // steeper -> drier
  float wetness  = saturate(shore + jitter - slope * 0.5);
  float dryness  = 1.0 - wetness;

  // Macro variation & flow streaks (stretch along river direction = TexCoord.y)
  float macro    = fbm(uv * 0.8);
  float streaks  = fbm(vec2(TexCoord.y * 6.0 + macro * 0.7, TexCoord.x * 0.6 - time * 0.03));
  streaks = pow(saturate(streaks), 3.0);              // thin erosion lines

  // Small-scale grit / pebbles
  float grit     = noise(uv * 18.0);
  float pebbles  = smoothstep(0.78, 0.96, noise(uv * 9.0 + 17.3));

  // --- Albedo synthesis ---
  // Start with a wet->dry soil ramp
  vec3 soilWet  = mix(wetSoil, dampSoil, saturate(macro * 0.6 + 0.2));
  vec3 soilDry  = mix(dampSoil, drySoil,  saturate(macro * 0.5 + 0.25));
  vec3 base     = mix(soilWet, soilDry, dryness);

  // Grass only where it’s drier, patchy from macro noise
  float grassMask = smoothstep(0.55, 0.95, dryness) * smoothstep(0.25, 0.75, fbm(uv * 1.2 + 5.1));
  base = mix(base, mix(base, grassTint, 0.65), grassMask);

  // Darker wet band right at the shoreline with soft, noisy edge
  float shoreBand = smoothstep(0.15, 0.85, wetness) * (0.6 + 0.4 * fbm(uv * 4.0));
  base = mix(base, mix(base, wetSoil, 0.75), shoreBand);

  // Grit/pebble breakup (subtle)
  base *= (0.9 + 0.2 * grit);
  base = mix(base, base * 0.8 + vec3(0.05), pebbles * (0.15 + 0.35 * dryness));

  // Cool tint near the water to sell dampness (very subtle)
  base = mix(base, base * vec3(0.92, 0.96, 1.00), wetness * 0.10);

  // --- Lighting ---
  vec3 L = normalize(vec3(0.3, 0.8, 0.4));
  vec3 V = normalize(vec3(0.0, 1.0, 0.5));
  vec3 N = normalize(Normal);

  float NdotL = max(dot(N, L), 0.0);
  vec3  ambient = vec3(0.40, 0.42, 0.44);

  // Wet areas are a bit darker (moisture darkening)
  float darken = mix(1.0, 0.82, wetness);
  vec3  diffuseCol = base * darken;

  // Diffuse + ambient
  vec3 color = diffuseCol * (ambient + NdotL * vec3(0.65, 0.62, 0.58));

  // Wet specular: tighter & stronger when wet, almost none when dry
  vec3  H = normalize(L + V);
  float shininess = mix(8.0, 48.0, wetness);                // tighter on wet
  float spec = pow(max(dot(N, H), 0.0), shininess);
  spec *= (0.15 + 0.55 * wetness);                          // only really visible when wet
  // Slightly cool, muddy spec color
  color += spec * vec3(0.25, 0.27, 0.30);

  // Add faint dark erosional streaks aligned with river flow
  color *= (1.0 - streaks * 0.08 * (0.4 + 0.6 * wetness));

  FragColor = vec4(saturate(color), 1.0);
}
