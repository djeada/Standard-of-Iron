# Minimap Static Layer Implementation (Stage I)

This directory contains the implementation of the **static layer** of the minimap system, which generates a single background texture from map JSON data.

## Overview

The minimap system has three conceptual layers:

1. **Static Layer (Implemented Here)** - Map background texture generated once per map
2. **Semi-Dynamic Layer (Future)** - Fog of war mask updated less frequently
3. **Dynamic Layer (Future)** - Interactive entities and UI updated frequently

This implementation focuses on **Stage I: Static Layer**.

## Components

### MinimapGenerator

**File:** `minimap_generator.h`, `minimap_generator.cpp`

The core class that generates a minimap texture from a `MapDefinition`. It converts JSON map data into a rasterized image suitable for OpenGL upload.

**Features:**
- Terrain base color rendering from biome settings
- Terrain features (hills, mountains) as shaded symbols
- Rivers as blue polylines
- Roads as brown paths
- Structures (barracks) as colored squares

**Usage:**
```cpp
#include "map/minimap/minimap_generator.h"

Game::Map::Minimap::MinimapGenerator generator;
QImage minimap = generator.generate(mapDefinition);
```

**Configuration:**
```cpp
Game::Map::Minimap::MinimapGenerator::Config config;
config.pixels_per_tile = 2.0F;  // Resolution scale
Game::Map::Minimap::MinimapGenerator generator(config);
```

### MinimapTextureManager

**File:** `minimap_texture_manager.h`, `minimap_texture_manager.cpp`

A higher-level manager class that handles the complete lifecycle of minimap textures, including OpenGL texture management.

**Features:**
- Automatic minimap generation from map definition
- OpenGL texture management (ready for integration)
- Image caching for debugging/saving

**Usage:**
```cpp
#include "map/minimap/minimap_texture_manager.h"

Game::Map::Minimap::MinimapTextureManager manager;
if (manager.generateForMap(mapDefinition)) {
    auto* texture = manager.getTexture();
    // Use texture for rendering
}
```

## Pipeline

The minimap generation follows this pipeline:

```
JSON Map File
     ↓
MapDefinition (parsed)
     ↓
MinimapGenerator
     ↓
QImage (RGBA)
     ↓
OpenGL Texture
```

### Rendering Order

Layers are rendered back-to-front:
1. Terrain base (biome color)
2. Roads
3. Rivers
4. Terrain features (hills, mountains)
5. Structures (barracks)

## Color Mapping

### Terrain Base
- Uses `biome.grass_primary` from map definition
- Fills the entire background

### Terrain Features
- **Mountains**: Dark gray/brown `(120, 110, 100)`
- **Hills**: Light brown `(150, 140, 120)`
- **Flat**: Slightly darker grass `(80, 120, 75)`

### Rivers
- Light blue `(100, 150, 255)` with alpha
- Width scales with `pixels_per_tile`

### Roads
- Brownish `(139, 119, 101)` with alpha
- Width scales with `pixels_per_tile`

### Structures
- **Player 1**: Blue `(100, 150, 255)`
- **Player 2**: Red `(255, 100, 100)`
- **Neutral**: Gray `(200, 200, 200)`

## Integration Guide

### During Map Load

Integrate minimap generation during map initialization:

```cpp
// In your map loading code (e.g., world_bootstrap.cpp)
#include "map/minimap/minimap_texture_manager.h"

// After loading map definition
Game::Map::Minimap::MinimapTextureManager minimap;
if (!minimap.generateForMap(mapDefinition)) {
    qWarning() << "Failed to generate minimap";
}

// Store minimap manager in your game state
// Use minimap.getTexture() for rendering in UI
```

### In Renderer

```cpp
// In your UI rendering code
auto* minimapTexture = gameState->getMinimapTexture();
if (minimapTexture) {
    minimapTexture->bind();
    // Render to screen
    minimapTexture->unbind();
}
```

## Testing

Comprehensive unit tests are provided in `tests/map/minimap_generator_test.cpp`:

```bash
# Run tests
cd build
./bin/standard_of_iron_tests --gtest_filter="MinimapGeneratorTest.*"
```

## Example

See `minimap_example.cpp` for a complete standalone example showing:
1. Loading a map from JSON
2. Generating a minimap
3. Saving the minimap to disk
4. Using the texture manager

## Performance

- **Generation Time**: Milliseconds for typical maps (< 100ms for 120x120 grid)
- **Memory**: Depends on resolution. Default 2 pixels/tile = ~100KB for 100x100 grid
- **Runtime Cost**: Zero - texture is generated once during map load

## Configuration

### Resolution Scaling

Adjust `pixels_per_tile` to control minimap resolution:

```cpp
config.pixels_per_tile = 1.0F;  // Lower resolution (faster, smaller)
config.pixels_per_tile = 2.0F;  // Default
config.pixels_per_tile = 4.0F;  // Higher resolution (slower, larger)
```

### Custom Colors

To customize colors, modify the color constants in `minimap_generator.cpp`:
- `biomeToBaseColor()` - Terrain base color
- `terrainFeatureColor()` - Feature colors
- `renderRivers()` - River color
- `renderRoads()` - Road color
- `renderStructures()` - Structure colors

## Future Enhancements (Not in This Stage)

- **Stage II**: Fog of war mask generation
- **Stage III**: Dynamic entity rendering
- Historical styling (parchment texture, hand-drawn icons)
- Borders and faction territories
- Elevation shading
- Mini-icons for villages and landmarks

## Technical Notes

### Coordinate System

- Input: World coordinates from `MapDefinition`
- Output: Pixel coordinates in texture
- Conversion: `pixel = world_coord * pixels_per_tile`

### Image Format

- **QImage Format**: RGBA8888
- **OpenGL Format**: GL_RGBA, GL_UNSIGNED_BYTE
- **Alpha Channel**: Used for semi-transparent features

### Anti-aliasing

QPainter anti-aliasing is enabled for smooth lines and curves.

## Dependencies

- Qt6 Core (QImage, QPainter, QColor)
- Qt6 Gui (QVector3D)
- Game map system (MapDefinition, terrain types)
- Render GL (Texture class for OpenGL integration)

## License

Part of the Standard of Iron project. See main project LICENSE.
