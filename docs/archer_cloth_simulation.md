# Archer Cloth Simulation

## Overview

The archer mesh has been enhanced with a dynamic tunic skirt that simulates cloth physics in real-time using a procedural rendering approach. This implementation provides believable secondary motion without requiring pre-baked animations or complex GPU compute shaders.

## Architecture

### Procedural Approach

Unlike traditional mesh-based cloth simulation, the archer uses a **procedural rendering system** where geometry is generated at runtime from primitive shapes (cylinders, spheres, cones). The tunic skirt is procedurally generated with the following components:

- **Segments**: 12 radial segments around the waist for smooth appearance
- **Structure**: Each segment consists of vertical strands from waist to hem, with horizontal connections
- **Double-layered**: Outer layer for main cloth, inner layer for thickness/depth

### Physics Simulation

The cloth simulation is entirely CPU-based and stateless, calculated per-frame based on:

1. **Animation Time**: Drives periodic motion
2. **Character Movement**: Influences cloth sway direction
3. **Wind Effects**: Adds environmental motion
4. **Collision Detection**: Prevents clipping with legs

## Implementation Details

### Key Parameters

```cpp
// Geometry
const float skirtLength = 0.25f;           // Length from waist to hem
const float waistRadius = HP::TORSO_BOT_R * 1.05f;  // Waist circumference
const float hemRadius = HP::TORSO_BOT_R * 1.35f;    // Hem circumference (flared)
const int segments = 12;                   // Radial segments

// Physics
const float windStrength = 0.18f;          // Wind influence magnitude
const float swayAmount = 0.05f;            // Maximum sway at hem
const float stiffness = 4.0f;              // Spring stiffness
const float damping = 0.4f;                // Damping coefficient

// Collision
const float legPushRadius = HP::UPPER_LEG_R * 1.6f;  // Leg avoidance radius
```

### Motion Calculation

#### 1. Wind and Character Velocity
```cpp
// Simulated wind direction (varies over time)
float windX = sin(windPhase) * windStrength;
float windZ = cos(windPhase * 0.7f) * windStrength;

// Character movement influence (when walking)
float characterVelX = isMoving ? sin(animTime * 3.0f) * 0.5f : 0.0f;
float characterVelZ = isMoving ? cos(animTime * 3.0f) * 0.5f : 0.0f;

// Combined swing direction (wind + opposite of movement)
float swingX = windX - characterVelX * 0.8f;
float swingZ = windZ - characterVelZ * 0.8f;
```

#### 2. Spring Physics (Damped Oscillation)
```cpp
// Per-vertex random phase offset for variation
float randomSeed = hash01(seed ^ (i * 7919u));
float phase = animTime * stiffness + randomSeed * 6.28f;

// Damped sinusoidal response
float lag = exp(-damping) * sin(phase);

// Additional flutter for detail
float flutter = sin(animTime * 10.0f + randomSeed * 6.28f) * 0.02f;

// Final offset
float offsetX = (swingX * swayAmount + flutter) * lag;
float offsetZ = (swingZ * swayAmount + flutter) * lag;
```

#### 3. Collision Avoidance
```cpp
// Check distance to each foot position
float distToLeg = sqrt(pow(hem.x - foot.x, 2) + pow(hem.z - foot.z, 2));

// If penetrating, push out radially
if (distToLeg < legPushRadius) {
    QVector3D pushDir = (hem - footPos).normalized();
    hem += pushDir * (legPushRadius - distToLeg);
}
```

### Visual Enhancement

#### Fabric Shading (basic.frag)

The shader includes special handling for cloth materials (bright colors):

```glsl
// Fabric weave pattern
float weaveX = sin(worldPos.x * 50.0);
float weaveZ = sin(worldPos.z * 50.0);
float weavePattern = weaveX * weaveZ * 0.02;

// Subtle noise variation
float clothNoise = noise(uv * 2.0) * 0.08 - 0.04;

// View-dependent sheen (highlights on edges)
float viewAngle = abs(dot(normal, viewDir));
float sheen = pow(1.0 - viewAngle, 3.0) * 0.12;

// Final color
variation = baseColor * (1.0 + clothNoise + weavePattern) + vec3(sheen);
```

#### Wrap Diffuse Lighting

Softer lighting terminator for cloth appearance:

```glsl
float wrapAmount = avgColor > 0.65 ? 0.5 : 0.0;  // Only for bright/cloth colors
float nDotL = dot(normal, lightDir);
float diff = max(nDotL * (1.0 - wrapAmount) + wrapAmount, 0.2);
```

#### Thickness Rendering

Inner layer provides depth perception:

```cpp
// Inner layer offset inward with slight vertical adjustment
QVector3D innerPoint = outerPoint - (radialDir * 0.01f) + QVector3D(0, 0.002f, 0);

// Render with darker color and slight transparency
out.mesh(..., C.tunic * 0.78f, nullptr, 0.85f);
```

## Performance Considerations

- **CPU-Based**: All calculations done on CPU per frame
- **Per-Instance**: Each archer individual has independent cloth simulation
- **Segment Count**: 12 segments provide good quality without excessive geometry
- **No State**: Stateless simulation requires no memory buffers or GPU compute

### Optimization Opportunities

If performance becomes an issue:

1. **Reduce Segments**: Lower from 12 to 8 for less draw calls
2. **LOD System**: Simplify or remove cloth for distant archers
3. **Update Rate**: Update cloth at lower frequency than render rate
4. **Instancing**: Share cloth simulation across similar movement states

## Usage

The cloth simulation is automatically applied to all archer units. No external configuration is needed.

### Tweaking Parameters

To adjust cloth behavior, modify constants in `archer_renderer.cpp`:

- **More Flutter**: Increase flutter amplitude or frequency
- **Stiffer Cloth**: Increase `stiffness` value
- **More Damping**: Increase `damping` for less oscillation
- **Longer Skirt**: Increase `skirtLength`
- **Wider Hem**: Increase `hemRadius` multiplier

## Future Enhancements

Potential improvements for more advanced cloth simulation:

1. **GPU Compute Shader**: Move simulation to GPU for better performance
2. **State Buffers**: Track velocity/position history for true inertia
3. **More Collision Primitives**: Add capsule collision for thighs
4. **Vertex Attributes**: If switching to mesh-based rendering
5. **Wind Zones**: Spatial wind variation across the map
6. **Material Variants**: Different cloth stiffness per unit type

## Related Files

- `render/entity/archer_renderer.cpp` - Main cloth rendering implementation
- `assets/shaders/basic.frag` - Fabric material shading
- `render/geom/transforms.h` - Geometry utility functions

## References

The implementation is inspired by the issue requirements but adapted for the procedural rendering architecture:

- Spring physics simulation (stateless per-vertex)
- Wind and character velocity influence
- Basic collision avoidance
- Fabric material appearance (sheen, wrap diffuse)
- Thickness rendering via inner layer
