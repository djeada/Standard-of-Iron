# Backend Module Structure

This directory contains the modular implementation of the OpenGL rendering backend.

## Architecture

The Backend class serves as the main rendering interface, but its implementation
is split across multiple files for better maintainability:

### Core Files
- `../backend.h` - Public Backend interface (in parent directory)
- `../backend.cpp` - Main implementation and command execution (in parent directory)

### Pipeline Implementations

The following pipeline modules reduce backend.cpp complexity (2055 → 973 lines, -53%):

- ✅ `cylinder_pipeline.h/.cpp` - Cylinder and fog-of-war instanced rendering (350 lines)
- ✅ `terrain_pipeline.h/.cpp` - Ground plane, terrain chunks, and grass instanced rendering (220 lines)
- ✅ `vegetation_pipeline.h/.cpp` - Trees (stone, plant, pine) and firecamp instanced rendering (533 lines)
- ✅ `character_pipeline.h/.cpp` - Character rendering (archer, swordsman, spearman, basic) (115 lines)
- ✅ `water_pipeline.h/.cpp` - River, riverbank, and bridge rendering (90 lines)
- ✅ `effects_pipeline.h/.cpp` - Grid, selection rings, selection smoke (75 lines)

### Utilities
- `pipeline_interface.h` - Abstract base class for all pipeline implementations
- `backend_impl.h` - Helper function declarations (forward compatibility)

## Design Principles

1. **Separation of Concerns**: Each pipeline handles one category of rendering
2. **Uniform Validation**: All pipelines validate uniform names match shaders
3. **Resource Management**: Each pipeline manages its own VAOs, VBOs, and shaders
4. **Testability**: Pipelines can be tested independently

## Naming Conventions

**IMPORTANT**: Shader uniforms use camelCase (e.g., `u_viewProj`, `u_lightDir`).
The backend code MUST use identical names when calling `uniformHandle()`.

Run the validation script to check for mismatches:
```bash
python3 scripts/validate_shader_uniforms.py
```

## Migration Status

- [x] Directory structure created
- [x] Pipeline interface defined  
- [x] Validation tooling added
- [x] Cylinder pipeline extracted (350 lines)
- [x] Vegetation pipeline extracted (533 lines)
- [x] Terrain pipeline extracted (220 lines)
- [x] Character pipeline extracted (115 lines)
- [x] Water pipeline extracted (90 lines)
- [x] Effects pipeline extracted (75 lines)
- [x] **Backend.cpp refactoring COMPLETE** (2055 → 973 lines, -53%)

## Adding New Pipelines

When creating a new rendering pipeline:

1. Inherit from `IPipeline` interface
2. Implement `initialize()`, `shutdown()`, `cacheUniforms()`, `is_initialized()`
3. Add validation to ensure uniform names match shader files
4. Document any shader dependencies
5. Update this README with the new pipeline

## Debugging

If rendering doesn't work after changes:

1. Check shader compilation errors in console
2. Run `validate_shader_uniforms.py` to find naming mismatches
3. Enable OpenGL debug output
4. Verify VAO/VBO bindings are correct
5. Check uniform locations aren't `InvalidUniform` (-1)
