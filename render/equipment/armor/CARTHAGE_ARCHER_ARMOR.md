# Carthaginian Archer Armor System

## Overview

This implementation provides two distinct armor variants for Carthaginian archers:
- **Light Armor** (`armor_light_carthage`) - Linen linothorax for standard archers
- **Heavy Armor** (`armor_heavy_carthage`) - Leather linothorax for elite archers

## Key Features

### Torso-Following Geometry
- Armor built from **cylinder segments** that wrap around the torso
- **Elliptical cross-section** (front deeper than back) matching human anatomy
- Multiple **horizontal rings** connected by vertical segments
- Properly **hugs the archer's body** instead of appearing as a box

### Shared Architecture
- Both variants use the **same construction approach** (cylinder rings)
- Both align to the **same attachment frames** (torso and waist)
- Both share the **same helmet model** (`carthage_light` - leather cap)

### Minimal C++ Implementation
- Each armor variant has its own `.cpp/.h` file pair
- C++ code focuses on:
  - Building torso-conforming geometry from cylinders
  - Transform calculation via attachment frames
  - Material binding (color and roughness)
- No complex mesh manipulation - simple procedural geometry

### Shader-Driven Complexity
All visual differentiation is handled by the existing `archer_carthage` shaders:

#### Vertex Shader Features
- **Depth offset**: 0.8cm normal offset to avoid z-fighting (line 39)
- **Leather tension**: Noise-driven perturbations for fabric stretch (lines 59-60)
- **Tangent/bitangent**: For anisotropic leather highlights (lines 31-35)
- **Segment encoding**: `v_armorLayer` for torso vs waist differentiation (lines 50-57)

#### Fragment Shader Features
- **Material detection**: Automatic detection via color ranges:
  - Leather: avgColor 0.25-0.55, r > g × 1.05
  - Linen: avgColor 0.68-0.90, r > b × 1.04
- **Procedural leather**: Triplanar FBM noise for grain (lines 115-116)
- **Linen weave**: Warp/weft patterns for fabric (lines 135-150)
- **Seam stitching**: Sinusoidal masks along UV seams (line 119)
- **Environmental effects**:
  - Rain darkening based on surface orientation (line 121)
  - Edge wear via curvature approximation (line 122)
- **Fresnel specular**: F₀ ≈ 0.04 for waxed leather (line 126)

## Usage

### Configuring Armor Variants

Edit `archer_style.cpp` to set the armor variant:

```cpp
void register_carthage_archer_style() {
  ArcherStyleConfig style;
  // ... other style settings ...
  
  // Choose armor variant:
  style.armor_id = "armor_light_carthage";  // Light linen linothorax
  // OR
  style.armor_id = "armor_heavy_carthage";  // Heavy leather
  
  register_archer_style("carthage", style);
}
```

### Color Ranges for Shader Detection

**Light Armor (Linen)**:
```cpp
QVector3D linen_color = QVector3D(0.85F, 0.80F, 0.72F);
float roughness = 0.8F; // High roughness for fabric
```

**Heavy Armor (Leather)**:
```cpp
QVector3D leather_color = QVector3D(0.42F, 0.32F, 0.24F);
float roughness = 0.6F; // Medium roughness for treated leather
```

## Technical Details

### Torso-Following Construction

The armor is built from horizontal cylinder rings that follow the torso's elliptical shape:

```cpp
// Create horizontal ring at specific height
auto createArmorRing = [&](float y_pos, float width_scale, 
                           float depth_front, float depth_back) {
  for (int i = 0; i < segments; ++i) {
    // Calculate positions around ellipse
    float r = width_scale * (abs(cos(angle)) * 0.25 + 0.75 * depth);
    QVector3D p = origin + right * (r * sin) + forward * (r * cos) + up * y;
    
    // Submit small cylinder segment
    submitter.mesh(getUnitCylinder(), cylinderBetween(p1, p2, radius), ...);
  }
};
```

Rings are created at:
- Shoulder level (y_top)
- Mid-chest (y_mid)
- Chest (y_chest)  
- Waist (y_waist)

Vertical connectors fill gaps between rings to create a solid shell.

### Elliptical Cross-Section

The armor follows human anatomy with deeper front (chest) than back:
- **Front depth**: 1.08-1.12 × torso radius
- **Back depth**: 0.85-0.88 × torso radius
- **Width taper**: Shoulder (1.18×) → Chest (1.12×) → Waist (1.08×)

### No Mesh Duplication

- Cylinders calculated procedurally each frame based on attachment frames
- No vertex buffer manipulation
- Shader handles all visual complexity
- Clean separation between geometry (C++) and appearance (shaders)

## Testing

Test coverage includes:
- Registration verification for both armor variants
- Shared helmet verification
- Equipment category separation validation

Run tests with:
```bash
./build/tests/render_tests --gtest_filter="ArmorRendererTest.*Carthage*"
```

## Files

### Header Files
- `render/equipment/armor/armor_light_carthage.h`
- `render/equipment/armor/armor_heavy_carthage.h`

### Implementation Files
- `render/equipment/armor/armor_light_carthage.cpp`
- `render/equipment/armor/armor_heavy_carthage.cpp`

### Shaders (Shared)
- `assets/shaders/archer_carthage.vert`
- `assets/shaders/archer_carthage.frag`

### Tests
- `tests/render/armor_renderer_test.cpp`
