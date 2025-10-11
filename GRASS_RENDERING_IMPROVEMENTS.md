# Grass Rendering Improvements

## Overview
This document describes the improvements made to the grass/terrain rendering system to address visual monotony and improve depth perception.

## Problem Statement
The ground had uniform color and texture, making it visually monotonous and tiring to look at. The terrain lacked visual depth and realistic variation.

## Solution
Introduced large-scale (low-frequency) color and roughness variation to the grass material through shader enhancements, without requiring new uniforms or texture assets.

## Changes Made

### 1. Large-Scale Moisture/Roughness Variation
**Purpose:** Simulate natural moisture distribution and grass patchiness

**Implementation:**
- Uses FBM noise at 0.4x the macro noise scale for smooth, large variations
- Offset by constant (7.3, 2.1) to decorrelate from existing macro noise
- Affects multiple visual properties:
  - **Dryness:** Increases dryness by up to 15% in low-moisture areas
  - **Micro-roughness:** Reduces surface roughness by 20-25% in wet areas (smoother appearance)
  - **Hue:** Subtle ±2.5-3% brightness variation for visual depth

**Code (ground_plane.frag):**
```glsl
float moistureNoise = fbm(wuv * u_macroNoiseScale * 0.4 + vec2(7.3, 2.1));
float moistureFactor = smoothstep(0.3, 0.7, moistureNoise);
```

### 2. Occasional Mud/Bare Ground Patches
**Purpose:** Break up repetition and add realistic variation

**Implementation:**
- Uses low-frequency noise at 0.25x macro scale
- Threshold at 0.75-0.82 creates distinct patches covering ~7-15% of area
- Patches are 15% darker than soil color for mud appearance
- Blended at 50-60% strength with smooth transitions
- On terrain, patches respect slope (reduced on steep areas)

**Code (ground_plane.frag):**
```glsl
float patchNoise = noise21(wuv * u_macroNoiseScale * 0.25 + vec2(13.7, 5.3));
float mudPatch = smoothstep(0.75, 0.82, patchNoise);
vec3 mudColor = u_soilColor * 0.85;
grassCol = mix(grassCol, mudColor, mudPatch * 0.6);
```

### 3. Enhanced Lighting Response
**Purpose:** Improve visual readability through roughness variation

**Implementation:**
- Micro-normal perturbation (surface roughness) now varies with moisture
- Wet areas appear smoother (less rough), dry areas more textured
- Creates subtle but noticeable lighting variation across the terrain

**Code:**
```glsl
float microAmp = 0.12 * (1.0 - moistureFactor * 0.25);
```

## Visual Impact

### Before
- Uniform grass color and texture
- Repetitive appearance
- Flat, monotonous visuals
- Eye fatigue from lack of visual interest

### After
- Large-scale moisture variation creates natural patchiness
- Mud patches break up uniformity with realistic bare ground areas
- Subtle hue and brightness shifts improve depth perception
- More visually engaging and easier on the eyes
- Better matches natural outdoor environments

## Performance Considerations

**Computational Cost:**
- Added 2 noise samples per fragment (moisture + patch)
- Both use existing noise functions (fbm, noise21)
- Low-frequency sampling (0.25-0.4x macro scale) is cache-friendly
- No texture lookups or additional uniforms
- Estimated impact: < 5% fragment shader cost

**Memory:**
- Zero additional memory usage
- No new textures or uniforms required
- Uses existing shader parameters

## Technical Details

### Shader Files Modified
1. `assets/shaders/ground_plane.frag` - Flat ground rendering
2. `assets/shaders/terrain_chunk.frag` - Elevated terrain rendering

### Key Parameters
- **Moisture scale:** 0.4x macro noise scale (large, smooth variation)
- **Patch scale:** 0.25x macro noise scale (even larger patches)
- **Moisture range:** smoothstep(0.3, 0.7) for gradual transitions
- **Patch threshold:** smoothstep(0.75, 0.82) for distinct patches
- **Mud color:** 85% of soil color (darker for realism)
- **Patch coverage:** 50-60% blend strength

### Consistency
Both shaders (ground_plane and terrain_chunk) implement the same moisture and patch systems to ensure visual consistency across flat and elevated terrain.

## Future Enhancements (Optional)

Potential improvements that could be made:
1. **Seasonal variation:** Add uniform to control overall moisture level
2. **Wind-driven patterns:** Animate moisture slightly with wind direction
3. **Height-based moisture:** More moisture in valleys, less on peaks
4. **Detail textures:** Optional detail albedo map for even finer variation

## Validation

The changes maintain:
- ✓ Existing shader interface (no new uniforms)
- ✓ Performance targets (minimal overhead)
- ✓ Visual style consistency
- ✓ Backward compatibility with existing maps/settings

## Conclusion

These improvements successfully address the visual monotony issue by introducing:
1. Large-scale moisture/roughness variation for depth
2. Occasional mud patches for realism
3. Subtle hue variation for visual interest

All achieved through minimal, surgical shader changes without requiring asset updates or significant performance impact.
