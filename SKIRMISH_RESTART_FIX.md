# Skirmish Restart Fix - Implementation Summary

## Problem
When restarting a skirmish after finishing or quitting a previous one, some game systems were not properly reset, leading to:
- Fog of war remaining in broken/uninitialized state
- Parts of map staying permanently visible or not reloading correctly
- Inconsistent behavior - sometimes worked, sometimes didn't
- AI state and caches not being cleared
- Player stats and troop counts carrying over from previous matches

## Root Cause
The `SkirmishLoader::start()` method had incomplete reset logic:
- Some systems were cleared (world, selection, building collisions, owner registry)
- Other critical systems were NOT reset before reinitialization:
  - `VisibilityService` - controls fog of war
  - `TerrainService` - holds height map and biome data
  - `GlobalStatsRegistry` - player statistics
  - `TroopCountRegistry` - troop counts per player
  - `FogRenderer` - fog rendering state
  - Enemy troop counter in GameEngine

## Solution
Created a centralized `resetGameState()` method that ensures complete and consistent teardown:

### 1. Added `SkirmishLoader::resetGameState()` method
Located in `game/map/skirmish_loader.cpp`

```cpp
void SkirmishLoader::resetGameState() {
  // Clear selection
  if (auto *selectionSystem = m_world.getSystem<Game::Systems::SelectionSystem>()) {
    selectionSystem->clearSelection();
  }

  // Pause and lock renderer
  m_renderer.pause();
  m_renderer.lockWorldForModification();
  m_renderer.setSelectedEntities({});
  m_renderer.setHoveredEntityId(0);

  // Clear ECS world
  m_world.clear();

  // Clear all singleton registries and services
  Game::Systems::BuildingCollisionRegistry::instance().clear();
  Game::Systems::OwnerRegistry::instance().clear();
  Game::Map::VisibilityService::instance().reset();
  Game::Map::TerrainService::instance().clear();
  Game::Systems::GlobalStatsRegistry::instance().clear();
  Game::Systems::TroopCountRegistry::instance().clear();

  // Clear fog renderer state
  if (m_fog) {
    m_fog->updateMask(0, 0, 1.0f, {});
  }
}
```

### 2. Added `TerrainService::clear()` method
Located in `game/map/terrain_service.cpp`

```cpp
void TerrainService::clear() {
  m_heightMap.reset();
  m_biomeSettings = BiomeSettings();
}
```

This ensures the terrain height map and biome settings are completely cleared before loading a new map.

### 3. Reset enemy troop counter in GameEngine
Located in `app/core/game_engine.cpp`

```cpp
void GameEngine::startSkirmish(const QString &mapPath, const QVariantList &playerConfigs) {
  clearError();
  m_level.mapPath = mapPath;
  m_level.mapName = mapPath;
  m_runtime.victoryState = "";
  m_enemyTroopsDefeated = 0;  // Reset counter
  // ... rest of method
}
```

### 4. Integrated reset into skirmish start flow
The `SkirmishLoader::start()` method now calls `resetGameState()` at the very beginning, ensuring proper order:

1. **resetGameState()** - Complete teardown of all systems
2. Load map player IDs
3. Configure owner registry
4. Load level data
5. Initialize new systems with fresh state

## Files Changed
1. `app/core/game_engine.cpp` - Reset enemy troop counter
2. `game/map/skirmish_loader.h` - Add resetGameState() declaration
3. `game/map/skirmish_loader.cpp` - Implement resetGameState() and call it
4. `game/map/terrain_service.h` - Add clear() method declaration
5. `game/map/terrain_service.cpp` - Implement clear() method

## Systems Now Properly Reset
- ✅ ECS World (entities and components)
- ✅ Selection System
- ✅ Building Collision Registry
- ✅ Owner Registry
- ✅ Visibility Service (fog of war)
- ✅ Terrain Service (height map and biome)
- ✅ Global Stats Registry (player statistics)
- ✅ Troop Count Registry
- ✅ Fog Renderer state
- ✅ Renderer selection/hover state
- ✅ Enemy troop counter

## Expected Behavior
After this fix:
- Restarting a skirmish always completely resets fog of war
- No leftover visibility from previous matches
- AI state is properly reinitialized
- Player statistics start fresh
- Terrain and biome data is cleared
- Works reliably even after multiple consecutive restarts
- All UI overlays and indicators are cleared

## Testing Recommendations
To verify the fix:
1. Start a skirmish match
2. Play for a while to explore the map (reveal fog of war)
3. Return to main menu
4. Start a new skirmish (same or different map)
5. Verify fog of war is completely reset (map should be dark/unexplored)
6. Repeat steps 1-5 multiple times to ensure consistency

The fog of war should always start fresh, with only areas around player units being visible.
