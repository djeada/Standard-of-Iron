# Victory Conditions

## Overview

Victory conditions in Standard of Iron are now configurable per map. Instead of being hardcoded to "barracks", maps can specify which unit type serves as the key structure for determining victory or defeat.

## Configuration

To configure a victory condition for a map, add the `victoryCondition` field to the map's JSON file:

```json
{
  "name": "My Custom Map",
  "victoryCondition": "barracks",
  "maxTroopsPerPlayer": 50,
  "grid": {
    ...
  },
  ...
}
```

## Default Behavior

If the `victoryCondition` field is not specified in the map JSON, the system defaults to "barracks" for backward compatibility with existing maps.

## Valid Victory Conditions

The victory condition must match a valid unit type name. Currently supported unit types include:
- `"barracks"` (default)
- `"archer"` (or any other registered unit type)

## Game Logic

The victory/defeat logic works as follows:

1. **Victory**: Achieved when all enemy key structures (specified by `victoryCondition`) are destroyed
2. **Defeat**: Occurs when all player key structures are destroyed

## Example Maps

### Standard Barracks Victory
```json
{
  "name": "Classic Battle",
  "victoryCondition": "barracks",
  ...
}
```

### Custom Victory Condition
If you create additional unit types in the future, you can set them as victory conditions:

```json
{
  "name": "Archer Survival",
  "victoryCondition": "archer",
  ...
}
```

## Implementation Details

- The victory condition is read from the map JSON during map loading
- It is stored in the `MapDefinition` struct and propagated through `LevelLoadResult` to the game engine
- The `checkVictoryCondition()` function uses this configured value instead of a hardcoded string
- Victory/defeat messages dynamically include the key structure type
