# Neutral (Unowned) Barracks Implementation

## Overview
This document describes the implementation of neutral barracks - barracks that exist on the map without an assigned owner.

## Neutral Owner ID
- Constant: `Game::Core::NEUTRAL_OWNER_ID = -1`
- Defined in: `game/core/ownership_constants.h`
- Helper function: `Game::Core::isNeutralOwner(int ownerId)`

## Key Implementation Points

### Map Loading (`game/map/map_loader.cpp`)
- The `readSpawns` function checks if `playerId` is present in the JSON
- If missing or null, assigns `NEUTRAL_OWNER_ID (-1)`
- Example JSON:
  ```json
  {
    "type": "barracks",
    "x": 50,
    "z": 50,
    "maxPopulation": 150
  }
  ```

### Map Transformation (`game/map/map_transformer.cpp`)
- Skips registering neutral spawns as players in the `OwnerRegistry`
- Neutral entities are not added to any team or player group

### Barracks Initialization (`game/units/barracks.cpp`)
- Checks `isNeutralOwner(m_u->ownerId)` before adding `ProductionComponent`
- Neutral barracks do NOT get a production component
- Still registered in `BuildingCollisionRegistry` for pathfinding/collision
- Still publishes `UnitSpawnedEvent` (with neutral owner ID)

### Visual Rendering (`game/visuals/team_colors.h`)
- `teamColorForOwner()` returns gray color (0.5, 0.5, 0.5) for neutral owners
- Neutral barracks appear visually distinct from player-owned barracks

### Production System (`game/systems/production_system.cpp`)
- Added safety check to skip entities with neutral ownership
- Since neutral barracks don't have `ProductionComponent`, this is defensive

### AI System (`game/systems/ai_system/behaviors/production_behavior.cpp`)
- Added explicit check to skip neutral barracks
- AI's `AISnapshot` only includes owned entities, so neutral barracks are already excluded
- Extra check added for safety and clarity

## Combat and Interaction
- Neutral barracks can be attacked (combat system allows it)
- They behave as enemies to all players (not allies with anyone)
- Future capture mechanics can change ownership from neutral to a player

## Testing
- Test map: `assets/maps/neutral_barracks_test.json`
- Contains barracks for player 1, neutral barracks, and barracks for player 2

## Future Enhancements
- Implement capture mechanics to claim neutral barracks
- Add visual indicators for neutral structures (flags, icons)
- Consider making neutral structures invulnerable until capture initiated
