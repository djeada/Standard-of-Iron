# Cursor and Hover Refactoring Summary

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
