# Standard-of-Iron RTS Game Engine

A modern real-time strategy (RTS) game engine built with C++20, Qt 6, and OpenGL 3.3 Core.

## Features

### Engine Architecture
- **Entity-Component-System (ECS)**: Flexible game object system with templated components
- **Event System**: Type-safe event management with subscription/publishing
- **Serialization**: JSON-based world and entity persistence
- **Multi-threaded Systems**: Separate systems for AI, combat, movement, pathfinding

### Rendering System
- **Modern OpenGL 3.3 Core**: Shader-based rendering pipeline
- **Batch Rendering**: Efficient batched draw calls with automatic sorting
- **Camera System**: RTS-style camera with perspective/orthographic projection
- **Mesh Generation**: Procedural mesh creation (primitives and terrain)
- **Texture Management**: Efficient texture loading and binding

### Game Systems
- **Movement System**: Smooth unit movement with pathfinding integration
- **Combat System**: Basic damage and health mechanics
- **AI System**: Extensible AI framework for unit behaviors  
- **Selection System**: Multi-unit selection with area selection
- **Pathfinding**: A* algorithm implementation for navigation

### User Interface
- **Qt Quick Integration**: Modern QML-based UI system
- **RTS HUD**: Pause/speed controls, resource display, minimap
- **Interactive Game View**: 3D rendering area with camera controls
- **Command Interface**: Unit selection panel and order buttons

### Development Tools
- **Map Editor**: Visual map creation and editing tool
- **Asset Pipeline**: Organized asset management system

## Requirements

- **C++20** compatible compiler (GCC 10+ or Clang 11+)
- **Qt 6.4+** with Quick, OpenGL modules
- **OpenGL 3.3+** support
- **CMake 3.21+**

## Building

### Linux

We currently support Ubuntu/Debian and Majaro/Arch.

```bash
# Clone
git clone https://github.com/djeada/Standard-of-Iron.git
cd Standard-of-Iron

# Install dependencies
make install

# Build
make build
```

### Running
```bash
# Main game
make run
```

## Project Structure

```
├── game/core/            # ECS, events, serialization
├── render/gl/            # OpenGL rendering system
├── game/systems/         # Game logic systems (AI, combat, movement)
├── assets/               # Game assets
│   ├── shaders/         # GLSL shaders
│   ├── textures/        # Texture files
│   ├── meshes/          # 3D models
│   ├── maps/            # Level data
│   └── units/           # Unit definitions
├── ui/qml/              # Qt Quick UI components
└── tools/map_editor/    # Level editing tool
```

## Controls

### Camera Controls
- **WASD**: Move camera
- **Mouse**: Look around
- **Scroll**: Zoom in/out
- **Q/E**: Rotate camera
- **R/F**: Move camera up/down

### Game Controls
- **Left Click**: Select unit/point
- **Drag**: Area selection
- **Right Click**: Issue orders
- **Space**: Pause/Resume
- **1/2/3**: Speed control

## Architecture Overview

### Entity-Component-System
The engine uses a modern ECS architecture where:
- **Entities** are unique IDs
- **Components** store data (Transform, Renderable, Unit, Movement)
- **Systems** process entities with specific component combinations

### Rendering Pipeline
1. **Scene Setup**: Camera and lighting configuration
2. **Culling**: Frustum culling for visible objects
3. **Batching**: Group draw calls by material/texture
4. **Rendering**: Execute batched draw calls
5. **Post-Processing**: UI overlay and effects

### Game Loop
1. **Input Processing**: Handle user input and events
2. **System Updates**: Run all game systems (AI, physics, etc.)
3. **Rendering**: Draw the current frame
4. **Audio/UI**: Update sound and interface

## Extending the Engine

### Adding New Components
```cpp
class MyComponent : public Engine::Core::Component {
public:
    float myData = 0.0f;
    std::string myString;
};
```

### Creating Custom Systems
```cpp
class MySystem : public Engine::Core::System {
public:
    void update(Engine::Core::World* world, float deltaTime) override {
        auto entities = world->getEntitiesWith<MyComponent>();
        for (auto entity : entities) {
            // Process entity
        }
    }
};
```

### Adding UI Elements
Edit the QML files in `ui/qml/` to customize the user interface.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Implement your changes
4. Add tests if applicable
5. Submit a pull request

## License

MIT License - see LICENSE file for details.

## Development Status

This is an active development project. Current focus areas:
- [ ] Networking for multiplayer
- [ ] Advanced AI behaviors
- [ ] Visual effects system
- [ ] Audio integration
- [ ] Level streaming
- [ ] Performance optimization
