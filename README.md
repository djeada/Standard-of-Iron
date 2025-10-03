# Standard of Iron - RTS Game

A modern real-time strategy (RTS) game built with C++20, Qt 6, and OpenGL 3.3 Core. Command archers and barracks in tactical battles with complete unit control, production systems, and victory conditions.

## Features

### Core Gameplay
- **Unit Production**: Build archers from barracks with production queues
- **Rally Points**: Set spawn locations for newly produced units (visual yellow flags)
- **Combat System**: Ranged archer combat with health bars and visual arrow projectiles
- **AI Opponents**: Computer-controlled enemy that produces units and attacks your base
- **Victory/Defeat**: Win by destroying the enemy barracks, lose if yours is destroyed
- **Team Colors**: Visual distinction between player (blue) and enemy (red) units

### Advanced Unit Commands
- **Move**: Right-click to move selected units to a location
- **Attack**: Use Attack command to force units to target enemies
- **Stop**: Halt all unit actions immediately
- **Patrol**: Set two waypoints for units to patrol (green flags), auto-engaging nearby enemies
- **Formation Planning**: Units automatically spread into formations when given group movement orders

### Selection & Control
- **Click Selection**: Click individual units or buildings
- **Area Selection**: Drag to select multiple units in a rectangle
- **Additive Selection**: Hold Shift to add to current selection
- **Smart Selection**: Selection system filters out dead units automatically
- **Selection Feedback**: Blue selection rings on selected units, hover rings on buildings

### Visual Systems
- **3D Rendering**: OpenGL-based rendering with custom shaders
- **Unit Visuals**: Distinct archer models with team-colored details
- **Building Visuals**: Detailed barracks with banners and production indicators
- **Health Bars**: Visible health for all units and buildings
- **Flag Markers**: Rally points (yellow) and patrol waypoints (green)
- **Arrow VFX**: Animated arrow projectiles with arcing trajectories
- **Terrain Grid**: Optional grid overlay for tactical planning

### Camera System
- **Free Movement**: WASD or arrow keys to pan camera
- **Rotation**: Q/E keys to rotate view
- **Elevation**: R/F keys to adjust height
- **Follow Mode**: Camera follows selected units (optional)
- **Edge Scrolling**: Move mouse to screen edges to pan
- **Smart UI Detection**: Edge scrolling disabled over HUD elements

### User Interface
- **Production Panel**: Shows building queue, progress, and unit counts
- **Command Buttons**: Context-sensitive controls (disabled when no units selected)
- **Speed Controls**: Pause and adjust game speed (0.5x to 2x)
- **Victory/Defeat Notification**: Clear on-screen messages for game end
- **Real-time Updates**: Production timers and status update live
- **Cursor Modes**: Visual feedback for attack, patrol, and guard modes

### Engine Architecture
- **Entity-Component-System (ECS)**: Flexible game object system with templated components
- **Event System**: Type-safe event management with subscription/publishing
- **Serialization**: JSON-based world and entity persistence
- **Multi-System Architecture**: Separate systems for AI, combat, movement, pathfinding, patrol, production

## Requirements

- **C++20** compatible compiler (GCC 10+ or Clang 11+)
- **Qt 6.4+** with Quick, OpenGL modules
- **OpenGL 3.3+** support
- **CMake 3.21+**

## Building

### Linux

We currently support Ubuntu/Debian and Manjaro/Arch.

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
├── game/
│   ├── core/              # ECS framework, components, world management
│   ├── systems/           # Game logic systems
│   │   ├── movement_system      # Unit movement and pathfinding
│   │   ├── combat_system        # Damage, health, and combat
│   │   ├── ai_system            # Enemy AI behavior
│   │   ├── patrol_system        # Patrol route management
│   │   ├── production_system    # Unit production and queues
│   │   ├── selection_system     # Unit/building selection
│   │   ├── arrow_system         # Arrow projectile VFX
│   │   └── ...
│   ├── map/               # Level loading and map data
│   ├── units/             # Unit type definitions (archer, barracks)
│   └── visuals/           # Visual configuration and team colors
├── render/
│   ├── gl/                # OpenGL rendering system
│   ├── entity/            # Entity-specific renderers
│   ├── geom/              # Geometry utilities (flags, arrows, selection)
│   └── ground/            # Terrain rendering
├── assets/
│   ├── shaders/           # GLSL shaders
│   ├── maps/              # Level data (JSON)
│   └── units/             # Unit definitions
├── ui/qml/                # Qt Quick UI components
│   ├── Main.qml           # Application window
│   ├── GameView.qml       # 3D game viewport
│   └── HUD.qml            # Heads-up display
└── app/                   # Application entry and game engine
```

## Controls

### Camera Controls
- **WASD** or **Arrow Keys**: Pan camera
- **Q/E**: Rotate camera left/right
- **R/F**: Move camera up/down
- **Mouse to Screen Edge**: Edge scrolling (disabled over UI)

### Selection Controls
- **Left Click**: Select unit or building
- **Click + Drag**: Area selection (draw rectangle)
- **Shift + Click**: Add to selection
- **Right Click**: Deselect all

### Unit Commands
- **Move**: Right-click on ground (selected units move there)
- **Attack**: Click Attack button, then click enemy target
- **Stop**: Press Stop button (halts movement, attack, and patrol)
- **Patrol**: Click Patrol button, click first waypoint, then second waypoint
- **Recruit**: Select barracks, click Recruit Archer button

### Game Controls
- **Space**: Pause/Resume
- **ESC**: Cancel current command mode (attack/patrol/guard)
- **Speed Slider**: Adjust game speed (0.5x - 2x)

### Keyboard Shortcuts (Camera)
- **W**: Pan forward
- **S**: Pan backward
- **A**: Pan left
- **D**: Pan right
- **Q**: Rotate left
- **E**: Rotate right
- **R**: Elevate camera
- **F**: Lower camera

## How to Play

### Objective
Destroy the enemy barracks while protecting your own.

### Basic Strategy
1. **Build Archers**: Select your barracks and recruit archers
2. **Set Rally Point**: Right-click with barracks selected to set spawn location
3. **Gather Forces**: Let archers accumulate at rally point
4. **Attack**: Select archers, click Attack, then click enemy barracks
5. **Defend**: Keep producing units to defend against enemy attacks

### Advanced Tactics
- **Patrol Routes**: Set up patrols to automatically defend areas
- **Formation Attacks**: Select multiple units before attacking for better damage
- **Rally Management**: Position rally points strategically (safe but close to action)
- **Production Timing**: Don't let your barracks sit idle
- **Resource Management**: Each barracks can only have 10 units maximum

## Architecture Overview

### Entity-Component-System
The engine uses a modern ECS architecture where:
- **Entities** are unique IDs representing game objects
- **Components** store data (Transform, Renderable, Unit, Movement, Health, Patrol, etc.)
- **Systems** process entities with specific component combinations each frame

### Key Components
- `TransformComponent`: Position, rotation, scale
- `UnitComponent`: Health, owner, unit type, speed, damage
- `MovementComponent`: Target position, pathfinding data
- `PatrolComponent`: Waypoints, patrol state
- `AttackTargetComponent`: Target entity, chase behavior
- `ProductionComponent`: Queue, timer, rally point
- `BuildingComponent`: Building-specific data

### Game Systems (Update Order)
1. **ArrowSystem**: Updates arrow VFX projectiles
2. **MovementSystem**: Moves units, executes pathfinding
3. **PatrolSystem**: Manages patrol routes, detects enemies
4. **CombatSystem**: Processes attacks, applies damage
5. **AISystem**: Controls enemy behavior
6. **ProductionSystem**: Handles unit spawning
7. **SelectionSystem**: Manages selection state

### Rendering Pipeline
1. **Scene Setup**: Camera matrices and viewport
2. **Ground Rendering**: Terrain with optional grid
3. **Entity Rendering**: All units and buildings (via entity-specific renderers)
4. **Arrow VFX**: Projectile effects
5. **Patrol Flags**: Waypoint markers (green) and rally points (yellow)
6. **UI Overlay**: HUD, selection indicators, health bars

## Extending the Game

### Adding New Unit Types
1. Create unit definition in `game/units/`
2. Add renderer in `render/entity/`
3. Register in entity renderer registry
4. Add to production service (optional)

### Creating Custom Commands
```cpp
// In game_engine.h
Q_INVOKABLE void onMyCommand(qreal sx, qreal sy);

// In game_engine.cpp
void GameEngine::onMyCommand(qreal sx, qreal sy) {
    // Convert screen to world coordinates
    QVector3D hit;
    if (!screenToGround(QPointF(sx, sy), hit)) return;
    
    // Issue command to selected units
    const auto& selected = m_selectionSystem->getSelectedUnits();
    for (auto id : selected) {
        // Process command...
    }
}
```

### Adding UI Elements
Edit QML files in `ui/qml/` to add new buttons, panels, or overlays.

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## Development Status

### Completed Features ✅
- Core ECS framework
- OpenGL rendering system
- Unit production and AI
- Combat and health systems
- Victory/defeat conditions
- Patrol system with visual waypoints
- Selection and command interface
- Rally point system
- Team colors and visual polish

### In Progress 🚧
- Guard command (stationary defense)
- Hold command (no chasing)
- Additional unit types

### Future Roadmap 🎯
- Multiplayer networking
- More unit types (melee, siege)
- Resource gathering system
- Multiple maps
- Campaign mode
- Advanced AI behaviors
- Save/load game state
- Sound effects and music

## License

MIT License - see LICENSE file for details.

## Acknowledgments

Built with modern C++20, Qt 6, and OpenGL 3.3 Core. Special thanks to the open-source community for excellent documentation and tools.
