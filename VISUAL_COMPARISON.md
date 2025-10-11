# Visual Comparison: Grass Rendering Improvements

## Change Summary

### 1. Moisture Variation System

**What it does:**
- Creates large-scale patches of "wetter" and "drier" grass across the terrain
- Wetter areas: greener, smoother surface, more uniform
- Drier areas: more yellow/brown tint, rougher surface, more varied

**Technical approach:**
```
moistureNoise = fbm(position * 1.8)  // Medium frequency = visible patches
moistureFactor = smoothstep(0.3, 0.7, moistureNoise)  // Smooth transitions

Effects:
- Dryness increases by 25% in low-moisture areas
- Surface roughness decreases by 25% in wet areas  
- Visible brightness variation of ±8% (ground) / ±6% (terrain)
```

**Visual result:**
```
Before:  [████████████████████████████████]  (uniform green)
After:   [██░░████░░░░████████░░██████░░██]  (varied moisture)
         └─wet  └─dry  └─wet └─dry └─wet
```

### 2. Mud Patch System

**What it does:**
- Adds occasional patches of bare ground / mud throughout the grass
- Appears as darker, desaturated areas
- Covers approximately 7-15% of flat terrain
- Reduced on slopes for natural appearance

**Technical approach:**
```
patchNoise = noise(position * 0.8)  // Medium-low frequency = visible patches
mudPatch = smoothstep(0.72, 0.80, patchNoise)  // Threshold for visible patches
mudColor = soilColor * 0.75  // 25% darker than regular soil

Result: Patches appear where noise > 0.72 (~15-20% of area)
```

**Visual result:**
```
Before:  [grass grass grass grass grass grass grass grass]

After:   [grass grass [mud] grass grass grass [mud] grass]
                  ▲                           ▲
              mud patches blend smoothly with grass
```

### 3. Roughness/Lighting Variation

**What it does:**
- Surface micro-detail (roughness) now varies with moisture
- Creates subtle lighting differences across terrain
- Wet areas catch more specular highlights
- Dry areas show more diffuse scattering

**Technical approach:**
```
Before: microAmp = 0.12  (constant)
After:  microAmp = 0.12 * (1.0 - moistureFactor * 0.25)

Result: 
- Wet areas: microAmp = 0.09 (smoother, less bumpy)
- Dry areas: microAmp = 0.12 (rougher, more textured)
```

### 4. Hue Variation

**What it does:**
- Adds very subtle color variation across the terrain
- Breaks up "sameness" even in areas of similar moisture
- ±3% brightness shift based on moisture pattern

**Technical approach:**
```
hueShift = (moistureNoise - 0.5) * 0.03  // Range: ±0.015
color *= (1.0 + jitter + hueShift)

Combined with existing jitter (±6%), creates natural color variation
```

## Expected Visual Changes

### Flat Grass Areas (ground_plane.frag)

**Before:**
- Uniform green color
- Consistent texture across entire plane
- Same brightness everywhere
- Repetitive, "artificial" appearance

**After:**
- Large patches of varied moisture (clearly visible color shifts)
- Noticeable darker mud/bare patches (~15-20% coverage)
- Visible brightness variation creating depth
- More "organic" and natural appearance

### Elevated Terrain (terrain_chunk.frag)

**Before:**
- Uniform grass on flat areas
- Predictable soil-to-grass transitions
- Same on all hills/plateaus

**After:**
- Moisture variation on plateaus and gentle slopes
- Mud patches primarily on flatter areas (reduced on steep slopes)
- More visual interest while maintaining gameplay readability
- Natural variation without obscuring terrain features

## Parameter Tuning

If the effect is too strong/weak, these values can be adjusted:

### Moisture Strength
```glsl
// Current: 0.25 (25% dryness increase) on ground, 0.20 on terrain
float dryness = clamp(0.3*detail + 0.25*(1.0-moistureFactor), 0.0, 0.6);
                                    ^^^^
// Increase for more variation: 0.35 (35%)
// Decrease for subtler: 0.15 (15%)
```

### Mud Patch Coverage
```glsl
// Current: 0.72-0.80 threshold (~15-20% coverage)
float mudPatch = smoothstep(0.72, 0.80, patchNoise);
                           ^^^^^  ^^^^
// More patches: 0.65-0.75 (~25-30% coverage)
// Fewer patches: 0.78-0.85 (~8-12% coverage)
```

### Mud Patch Strength
```glsl
// Current: 75% blend on ground, 65% on terrain, 25% darker
vec3 mudColor = u_soilColor * 0.75;
grassCol = mix(grassCol, mudColor, mudPatch * 0.75);
                                             ^^^^
// Stronger (darker patches): 0.85
// Subtler (lighter patches): 0.55
```

### Hue Variation
```glsl
// Current: ±8% on ground, ±6% on terrain
float hueShift = (moistureNoise - 0.5) * 0.08;  // ground
                                         ^^^^
// More variation: 0.12 (±12%)
// Less variation: 0.04 (±4%)
```

## Performance Notes

**Fragment Shader Cost:**
- 2 additional noise samples (moistureNoise, patchNoise)
- 4-5 additional arithmetic operations
- Estimated impact: 3-5% on fragment shader performance
- Still well within budget for this shader complexity

**When to notice:**
- Low-end GPUs: Minimal impact (shader is not the bottleneck)
- High-end GPUs: Imperceptible (plenty of ALU headroom)
- Fillrate-limited scenes: No impact (compute-bound, not memory-bound)

## Testing Checklist

When the application runs, verify:

- [ ] Grass shows large-scale color variation (not uniform)
- [ ] Occasional darker patches appear on flat areas
- [ ] Patches blend smoothly (no hard edges)
- [ ] Variation is consistent between ground plane and terrain
- [ ] Performance is acceptable (no frame rate drops)
- [ ] Variation enhances rather than distracts from gameplay
- [ ] Lighting still looks natural (no artifacts)

## Troubleshooting

**If grass looks too "splotchy":**
- Reduce mud patch strength (0.6 → 0.4)
- Reduce moisture dryness impact (0.15 → 0.10)

**If variation is barely visible:**
- Increase hue shift (0.03 → 0.05)
- Increase moisture dryness impact (0.15 → 0.20)
- Lower mud patch threshold (0.75 → 0.70)

**If patches look too harsh:**
- Widen smoothstep range (0.75-0.82 → 0.73-0.85)
- Reduce blend strength (0.6 → 0.5)

**If performance is impacted:**
- Simplify moisture calculation (use noise21 instead of fbm)
- Remove hue variation (minimal visual impact)
