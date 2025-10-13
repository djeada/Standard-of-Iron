# Refactoring Summary

## 1. World Bootstrap and Map Lifecycle Refactoring

### Overview
This refactoring separates world initialization, skirmish loading, and map catalog management from the `GameEngine` into clearly defined, modular services. The goal is to clarify the boundaries between engine setup, map I/O, and runtime world management—reducing coupling and improving testability of core loading logic.

### New Services Created

#### WorldBootstrap Class (game/map/world_bootstrap.h, game/map/world_bootstrap.cpp)
**Purpose**: Responsible for initializing world and rendering context.

**Responsibilities**:
- World and renderer initialization
- Ground renderer setup
- Error handling for initialization failures

**Public API**:
- `initialize(Renderer, Camera, GroundRenderer, outError)` → `bool`
- `ensureInitialized(initialized, Renderer, Camera, GroundRenderer, outError)` → `void`

**Benefits**:
- Clean entry point for creating a functional world ready for simulation and rendering
- Error handling is explicit and testable
- Decouples OpenGL setup from game logic

#### SkirmishLoader Class (game/map/skirmish_loader.h, game/map/skirmish_loader.cpp)
**Purpose**: Handles the full lifecycle of starting a skirmish map.

**Responsibilities**:
- Map I/O and parsing
- Owner registry setup
- Terrain and fog configuration
- Visibility initialization
- Camera focus calculation
- Renderer lock/unlock flow
- Player configuration and team management

**Public API**:
- `start(mapPath, playerConfigs, selectedPlayerId, outSelectedPlayerId)` → `SkirmishLoadResult`
- `setGroundRenderer(GroundRenderer*)` - Dependency injection
- `setTerrainRenderer(TerrainRenderer*)` - Dependency injection
- `setBiomeRenderer(BiomeRenderer*)` - Dependency injection
- `setFogRenderer(FogRenderer*)` - Dependency injection
- `setStoneRenderer(StoneRenderer*)` - Dependency injection
- `setOnOwnersUpdated(callback)` - Hook for owner registry updates
- `setOnVisibilityMaskReady(callback)` - Hook for visibility mask updates

**Benefits**:
- Cleanly separates data loading and game state setup from engine runtime
- All map loading concerns in one place
- Easy to test independently
- Clear callback hooks for state updates

#### MapCatalog Class (game/map/map_catalog.h, game/map/map_catalog.cpp)
**Purpose**: Manages map discovery and metadata.

**Responsibilities**:
- Reading available maps from `assets/maps/*.json`
- Extracting map metadata (name, description, player slots, thumbnails)
- Returning lightweight DTOs for UI consumption

**Public API**:
- `availableMaps()` → `QVariantList`

**Benefits**:
- Keeps file system and JSON parsing concerns out of the engine's core logic
- Single source of truth for map metadata
- Easy to extend with caching or filtering

### GameEngine Changes
**Removed** (~289 lines):
- `initialize()` method - moved to `WorldBootstrap`
- `availableMaps()` implementation - moved to `MapCatalog`
- `startSkirmish()` implementation - moved to `SkirmishLoader`
- All inline map file parsing logic
- Owner registry setup code
- Visibility service initialization
- Terrain configuration logic

**Added** (~44 lines):
- Delegation to `WorldBootstrap::ensureInitialized()`
- Delegation to `MapCatalog::availableMaps()`
- Instantiation and configuration of `SkirmishLoader`
- Callback hooks for owner and visibility updates

### Impact
- **GameEngine**: Reduced from complex map loading orchestrator to simple delegator
- **Testability**: Each service can be unit tested independently
- **Separation of Concerns**: Clear boundaries between initialization, loading, and cataloging
- **Maintainability**: Future map formats or loading strategies can be added without touching GameEngine

---

## 2. Cursor and Hover Refactoring Summary

## Overview
This refactoring decouples cursor and hover state management from the core `GameEngine` class into dedicated UI-focused components: `CursorManager` and `HoverTracker`.

## Changes Made

### 1. CursorManager Class (app/cursor_manager.h, app/cursor_manager.cpp)
**Purpose**: Manages all cursor-related state and logic.

**Responsibilities**:
- Cursor mode state (normal, patrol, attack)
- Cursor shape updates (Qt::ArrowCursor, Qt::BlankCursor)
- Patrol waypoint state (first waypoint for patrol mode)
- Global cursor position tracking

**Public API**:
- `setMode(QString)` - Sets the cursor mode
- `updateCursorShape(QQuickWindow*)` - Updates the Qt cursor shape
- `globalCursorX(QQuickWindow*)` - Returns cursor X position
- `globalCursorY(QQuickWindow*)` - Returns cursor Y position
- `hasPatrolFirstWaypoint()` - Check if patrol first waypoint is set
- `setPatrolFirstWaypoint(QVector3D)` - Set patrol first waypoint
- `clearPatrolFirstWaypoint()` - Clear patrol first waypoint
- `getPatrolFirstWaypoint()` - Get patrol first waypoint

**Signals**:
- `modeChanged()` - Emitted when cursor mode changes
- `globalCursorChanged()` - Emitted when cursor position changes

### 2. HoverTracker Class (app/hover_tracker.h, app/hover_tracker.cpp)
**Purpose**: Tracks which entity is currently under the cursor.

**Responsibilities**:
- Entity hover detection via PickingService
- Storing last hovered entity ID
- Boundary checking for viewport

**Public API**:
- `updateHover(sx, sy) -> EntityID` - Updates and returns the hovered entity
- `getLastHoveredEntity() -> EntityID` - Returns the last hovered entity ID

### 3. GameEngine Refactoring (app/game_engine.h, app/game_engine.cpp)
**Changes**:
- Removed `RuntimeState::cursorMode` and `RuntimeState::currentCursor`
- Removed `HoverState` struct
- Removed `PatrolState` struct
- Removed `updateCursor()` private method
- Added `m_cursorManager` member (unique_ptr<CursorManager>)
- Added `m_hoverTracker` member (unique_ptr<HoverTracker>)
- Modified `cursorMode()` to delegate to CursorManager
- Modified `setCursorMode()` to delegate to CursorManager
- Modified `globalCursorX()` to delegate to CursorManager
- Modified `globalCursorY()` to delegate to CursorManager
- Modified `setHoverAtScreen()` to use HoverTracker
- Modified `hasPatrolPreviewWaypoint()` to delegate to CursorManager
- Modified `getPatrolPreviewWaypoint()` to delegate to CursorManager
- Updated all cursor/hover state access to use the new managers

**Signal Forwarding**:
Connected CursorManager signals to GameEngine signals in constructor:
- `CursorManager::modeChanged` -> `GameEngine::cursorModeChanged`
- `CursorManager::globalCursorChanged` -> `GameEngine::globalCursorChanged`

### 4. Build Configuration (CMakeLists.txt)
Added new source files:
- `app/cursor_manager.cpp`
- `app/hover_tracker.cpp`

## Benefits

1. **Separation of Concerns**: UI interaction logic is now isolated from game logic
2. **Maintainability**: Cursor and hover behavior can be modified independently
3. **Testability**: CursorManager and HoverTracker can be tested in isolation
4. **Clarity**: GameEngine is now focused on game logic, not UI state
5. **Reusability**: Managers can potentially be reused in other parts of the application

## API Compatibility

All public APIs of GameEngine remain unchanged:
- Q_PROPERTY declarations are the same
- Signal names are the same
- Method signatures are the same
- Behavior is identical

The refactoring is completely transparent to QML and other GameEngine consumers.
