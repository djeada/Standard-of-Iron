# OwnerRegistry Service

## Overview

The OwnerRegistry is a globally available service that manages ownership information in the game. It replaces the hardcoded ownerId logic that was previously scattered throughout the codebase.

## Purpose

Previously, the game used hardcoded values:
- `1` for the player
- `2` for the AI

This approach was:
- **Confusing**: Magic numbers without clear meaning
- **Not scalable**: Cannot support multiple players or AI instances
- **Hard to maintain**: Changes required modifying code in multiple places

## Solution

The OwnerRegistry service provides:
- **Centralized ownership management**: Single source of truth for all owner information
- **Type-safe ownership**: Explicit OwnerType enum (Player, AI, Neutral)
- **Dynamic registration**: Owners are registered when the map loads
- **Query interface**: Easy methods to check owner type and properties

## API

### Core Methods

```cpp

int registerOwner(OwnerType type, const std::string &name = "");

void setLocalPlayerId(int playerId);

bool isPlayer(int ownerId) const;
bool isAI(int ownerId) const;
OwnerType getOwnerType(int ownerId) const;
std::string getOwnerName(int ownerId) const;

const std::vector<OwnerInfo> &getAllOwners() const;
std::vector<int> getPlayerOwnerIds() const;
std::vector<int> getAIOwnerIds() const;
```

## Usage Example

```cpp

auto &registry = Game::Systems::OwnerRegistry::instance();
registry.clear();

int playerOwnerId = registry.registerOwner(OwnerType::Player, "Player");
int aiOwnerId = registry.registerOwner(OwnerType::AI, "AI Player");

registry.setLocalPlayerId(playerOwnerId);

if (registry.isPlayer(someOwnerId)) {

}

if (registry.isAI(someOwnerId)) {

}
```

## UI Integration

The GameEngine exposes owner information to QML through the `ownerInfo` property:

```qml

Label {
    text: {
        var owners = game.ownerInfo
        var playerCount = 0
        var aiCount = 0
        for (var i = 0; i < owners.length; i++) {
            if (owners[i].type === "Player") playerCount++
            else if (owners[i].type === "AI") aiCount++
        }
        return "👥 " + playerCount + " | 🤖 " + aiCount
    }
}
```

Each owner object in the array contains:
- `id`: The owner ID
- `name`: Human-readable name
- `type`: "Player", "AI", or "Neutral"
- `isLocal`: Boolean indicating if this is the local player

## Migration Guide

### Before
```cpp
if (unit->ownerId == 1) {

}

if (unit->ownerId == 2) {

}

int playerId = 1;
```

### After
```cpp
auto &registry = Game::Systems::OwnerRegistry::instance();

if (registry.isPlayer(unit->ownerId)) {

}

if (registry.isAI(unit->ownerId)) {

}

int playerId = registry.getLocalPlayerId();
```

## Benefits

1. **Scalability**: Easy to add multiple players or AI instances
2. **Clarity**: Code explicitly states intent (e.g., `isPlayer()` vs `== 1`)
3. **Maintainability**: Single place to modify ownership logic
4. **UI Support**: Ownership information readily available for display
5. **Future-proof**: Foundation for multiplayer and advanced AI scenarios

## Files Modified

- `game/systems/owner_registry.h`: Service interface
- `game/systems/owner_registry.cpp`: Service implementation
- `game/map/map_transformer.cpp`: Uses registry instead of static variable
- `game/map/level_loader.cpp`: Uses registry for ownership checks
- `game/systems/ai_system.cpp`: Uses registry for AI player ID
- `app/game_engine.cpp`: Initializes registry and exposes to UI
- `ui/qml/HUDTop.qml`: Displays ownership information
