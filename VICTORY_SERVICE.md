# VictoryService Implementation

## Overview
The VictoryService is a standalone service that manages all victory and defeat conditions in the game. It replaces the hardcoded victory checks that were previously embedded in the GameEngine.

## Architecture

### Design Principles
1. **Data-Driven**: Victory conditions are defined in map JSON files, not in code
2. **Event-Based**: Uses the game's event system to react to unit deaths
3. **Decoupled**: Independent service that doesn't rely on GameEngine internals
4. **Extensible**: Easy to add new victory types and defeat conditions

### Key Components
- **VictoryService** (`game/systems/victory_service.h/cpp`): Core service implementation
- **VictoryConfig** (`game/map/map_definition.h`): Data structure for victory configuration
- **MapLoader** (`game/map/map_loader.cpp`): Parses victory config from JSON
- **LevelLoader** (`game/map/level_loader.h/cpp`): Passes victory config to game engine
- **GameEngine** (`app/game_engine.h/cpp`): Integrates VictoryService and handles callbacks

## Victory Types

### Elimination
Destroy all enemy key structures to win.

**Configuration:**
```json
"victory": {
  "type": "elimination",
  "key_structures": ["barracks", "HQ"],
  "defeat_conditions": ["no_key_structures"]
}
```

**Logic:**
- Checks if any enemy key structures are still alive
- Victory when all enemy key structures are destroyed
- Defeat when player has no key structures remaining

### Survive Time
Survive for a specified duration to win.

**Configuration:**
```json
"victory": {
  "type": "survive_time",
  "duration": 600,
  "defeat_conditions": ["no_units"]
}
```

**Logic:**
- Counts elapsed time each frame
- Victory when elapsed time >= duration
- Defeat when player has no units remaining

## Defeat Conditions

### No Key Structures
Lose if all key structures are destroyed.

**Example:**
```json
"defeat_conditions": ["no_key_structures"]
```

### No Units
Lose if all units are eliminated (including buildings).

**Example:**
```json
"defeat_conditions": ["no_units"]
```

## Integration Flow

1. **Map Loading**: MapLoader reads victory config from JSON
2. **Level Loading**: LevelLoader passes config to GameEngine via LevelLoadResult
3. **Configuration**: GameEngine configures VictoryService with map-specific settings
4. **Runtime**: VictoryService updates each frame, checking conditions
5. **Event Handling**: VictoryService subscribes to UnitDiedEvent for reactive checks
6. **Victory/Defeat**: Callback to GameEngine to update UI and game state

## Implementation Details

### Event Subscription
```cpp
VictoryService::VictoryService()
    : m_unitDiedSubscription([this](const Engine::Core::UnitDiedEvent &e) {
        onUnitDied(e);
      }) {}
```

The service subscribes to unit death events to potentially trigger immediate victory/defeat checks.

### Update Loop
```cpp
void VictoryService::update(Engine::Core::World &world, float deltaTime) {
  if (!m_victoryState.isEmpty()) return;
  
  if (m_victoryType == VictoryType::SurviveTime) {
    m_elapsedTime += deltaTime;
  }
  
  checkVictoryConditions(world);
  if (!m_victoryState.isEmpty()) return;
  
  checkDefeatConditions(world);
}
```

### Callback Pattern
```cpp
// In GameEngine::startSkirmish
m_victoryService->setVictoryCallback([this](const QString &state) {
  if (m_runtime.victoryState != state) {
    m_runtime.victoryState = state;
    emit victoryStateChanged();
  }
});
```

## Testing Scenarios

### Scenario 1: Standard Elimination
**Map Config:**
```json
"victory": {
  "type": "elimination",
  "key_structures": ["barracks"],
  "defeat_conditions": ["no_key_structures"]
}
```

**Test Steps:**
1. Start game with test_map.json
2. Attack and destroy enemy barracks
3. Verify victory message appears
4. Allow enemy to destroy player barracks
5. Verify defeat message appears

### Scenario 2: Survival Mode
**Map Config:**
```json
"victory": {
  "type": "survive_time",
  "duration": 60,
  "defeat_conditions": ["no_units"]
}
```

**Test Steps:**
1. Start game with survival map
2. Wait 60 seconds (1 minute)
3. Verify victory message after time elapsed
4. Start new game and lose all units
5. Verify defeat message appears

### Scenario 3: Multiple Key Structures
**Map Config:**
```json
"victory": {
  "type": "elimination",
  "key_structures": ["barracks", "HQ"],
  "defeat_conditions": ["no_key_structures"]
}
```

**Test Steps:**
1. Verify victory only when ALL key structure types are destroyed
2. Verify defeat when ANY key structure type is completely lost

## Future Extensions

### Potential Victory Types
- **Capture Points**: Hold specific map locations
- **Resource Collection**: Gather specified resources
- **Tech Victory**: Research all technologies
- **Diplomatic Victory**: Form alliances with all players

### Potential Defeat Conditions
- **Time Limit**: Lose if objective not met in time
- **No Production**: Lose if unable to produce units
- **Morale**: Lose if morale drops too low

### Implementation Pattern
```cpp
// In victory_service.h
enum class VictoryType { 
  Elimination, 
  SurviveTime, 
  CapturePoints,  // New type
  Custom 
};

// In victory_service.cpp
bool VictoryService::checkCapturePoints(Engine::Core::World &world) {
  // Check if player controls all capture points
  // Return true if victory condition met
}
```

## Benefits

### Before (Hardcoded in GameEngine)
- Victory logic mixed with engine code
- Required code changes for new game modes
- Difficult to test different scenarios
- Limited to barracks-only victory

### After (VictoryService)
- Victory logic isolated in dedicated service
- New game modes via JSON configuration
- Easy to test with different map configs
- Flexible, supports multiple structure types and modes
- Modding support without code changes
- Campaign missions with custom objectives

## Files Modified

### New Files
- `game/systems/victory_service.h`
- `game/systems/victory_service.cpp`

### Modified Files
- `game/map/map_definition.h` - Added VictoryConfig struct
- `game/map/map_loader.cpp` - Added readVictoryConfig function
- `game/map/level_loader.h` - Added victoryConfig to LevelLoadResult
- `game/map/level_loader.cpp` - Pass victory config from map
- `game/CMakeLists.txt` - Added victory_service.cpp to build
- `app/game_engine.h` - Added VictoryService member and forward declaration
- `app/game_engine.cpp` - Integrated VictoryService, removed old checkVictoryCondition
- `assets/maps/test_map.json` - Added victory configuration
- `assets/maps/two_player_map.json` - Added victory configuration
- `assets/maps/test_width_depth.json` - Added victory configuration
- `README.md` - Updated documentation with victory system info
