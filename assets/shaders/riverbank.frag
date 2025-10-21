#version 330 core
out vec4 FragColor;

in vec2 TexCoord; // x: 0 = water edge -> 1 = land side, y: along the river
in vec3 WorldPos;
in vec3 Normal;

uniform float time;

// ------------------------- utils -------------------------
float saturate(float x) { return clamp(x, 0.0, 1.0); }
vec2 saturate(vec2 v) { return clamp(v, vec2(0.0), vec2(1.0)); }
vec3 saturate(vec3 v) { return clamp(v, vec3(0.0), vec3(1.0)); }

float hash(vec2 p) {
  p = fract(p * vec2(123.34, 456.21));
  p += dot(p, p + 45.32);
  return fract(p.x * p.y);
}
float noise(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float a = hash(i), b = hash(i + vec2(1, 0)), c = hash(i + vec2(0, 1)),
        d = hash(i + vec2(1, 1));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
float fbm(vec2 p) {
  float v = 0.0, a = 0.5, f = 1.0;
  for (int i = 0; i < 5; i++) {
    v += a * noise(p * f);
    f *= 2.0;
    a *= 0.5;
  }
  return v;
}
vec2 warp(vec2 uv) {
  vec2 w1 = vec2(fbm(uv * 0.7 + time * 0.06), fbm(uv * 0.7 - time * 0.05));
  vec2 w2 = vec2(fbm(uv * 1.1 - time * 0.03), fbm(uv * 0.9 + time * 0.04));
  return uv + 0.22 * w1 + 0.12 * w2;
}

// ------------------------- main -------------------------
void main() {
  // World-anchored UVs to avoid tiling per mesh piece
  vec2 uv = warp(WorldPos.xz * 0.45);

  // --- Palettes (slightly lighter and more natural) ---
  vec3 wetSoil = vec3(0.20, 0.17, 0.14);
  vec3 dampSoil = vec3(0.32, 0.27, 0.22);
  vec3 drySoil = vec3(0.46, 0.40, 0.33);
  vec3 grassTint = vec3(0.30, 0.50, 0.25);

  // --- Wetness computation ---
  // Base width from cross-river coord (narrower; was too thick)
  float baseWet =
      smoothstep(0.30, 0.04, TexCoord.x); // 1 near water, 0 towards land

  // Irregular shoreline: jitter width with world-space FBM and along-flow noise
  float edgeJitter = (fbm(uv * 2.5) - 0.5) * 0.12 +
                     (fbm(vec2(TexCoord.y * 3.0, 0.0)) - 0.5) * 0.10;
  baseWet = saturate(baseWet + edgeJitter);

  // Slope-aware: steeper banks drain faster (less wet)
  float slope = 1.0 - saturate(normalize(Normal).y);
  float wetness = saturate(baseWet - slope * 0.45);

  // A super-narrow “water contact” band to soften the hard polygon edge
  // visually
  float contact =
      smoothstep(0.10, 0.95, wetness) * smoothstep(0.04, 0.00, TexCoord.x);
  contact *= 0.6 + 0.4 * fbm(uv * 4.0 + time * 0.2); // noisy, thin

  // --- Macro variation & flow-aligned streaks (erosion/silt) ---
  float macro = fbm(uv * 0.8);
  float streaks =
      fbm(vec2(TexCoord.y * 6.0 + macro * 0.7, TexCoord.x * 0.6 - time * 0.03));
  streaks = pow(saturate(streaks), 3.0); // thin dark streak lines

  // Small grit & sparse pebbles
  float grit = noise(uv * 18.0);
  float pebbles = smoothstep(0.82, 0.97, noise(uv * 9.0 + 17.3));

  // --- Albedo synthesis ---
  // Wet->dry soil ramp with macro tinting
  vec3 soilWet = mix(wetSoil, dampSoil, saturate(macro * 0.5 + 0.25));
  vec3 soilDry = mix(dampSoil, drySoil, saturate(macro * 0.5 + 0.25));
  vec3 base = mix(soilWet, soilDry, 1.0 - wetness);

  // Grass appears as it gets drier, patchy by macro noise
  float grassMask = smoothstep(0.50, 0.92, 1.0 - wetness) *
                    smoothstep(0.25, 0.75, fbm(uv * 1.2 + 5.1));
  base = mix(base, mix(base, grassTint, 0.6), grassMask);

  // Soft cool tint right at the contact band (reduces “brown outline”)
  base = mix(base, base * vec3(0.92, 0.96, 1.02), contact * 0.35);

  // Grit/pebble breakup (subtle, only really reads when dry)
  base *= (0.92 + 0.16 * grit);
  base = mix(base, base * 0.82 + vec3(0.05),
             pebbles * (0.10 + 0.35 * (1.0 - wetness)));

  // --- Lighting ---
  vec3 L = normalize(vec3(0.3, 0.8, 0.4));
  vec3 V = normalize(vec3(0.0, 1.0, 0.5));
  vec3 N = normalize(Normal);

  float NdotL = max(dot(N, L), 0.0);

  // Moisture darkening (lighter than before to avoid a black ring)
  float darken = mix(1.0, 0.94, wetness);

  vec3 ambient = vec3(0.42, 0.44, 0.46);
  vec3 color = base * darken * (ambient + NdotL * vec3(0.66, 0.64, 0.60));

  // Wet specular: only very near the water edge; cool tint
  vec3 H = normalize(L + V);
  float shininess = mix(10.0, 38.0, saturate(wetness * 1.2));
  float spec = pow(max(dot(N, H), 0.0), shininess);
  spec *= (0.10 + 0.50 * contact); // confined to the thin contact zone
  color += spec * vec3(0.30, 0.33, 0.36);

  // Flow-aligned silt darkening (very gentle)
  color *= (1.0 - streaks * 0.07 * (0.3 + 0.7 * wetness));

  FragColor = vec4(saturate(color), 1.0);
}
