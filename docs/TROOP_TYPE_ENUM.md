# TroopType Enum Usage Guide

## Overview

The game now uses a strongly-typed `TroopType` enum instead of hardcoded string literals for troop identifiers. This provides compile-time type safety and prevents typos.

## Quick Start

### Include the Header

```cpp
#include "game/units/troop_type.h"
```

### Using the Enum

```cpp
// Creating troops
Game::Units::TroopType myTroop = Game::Units::TroopType::Archer;

// Comparing troop types
if (myTroop == Game::Units::TroopType::Archer) {
    // Handle archer logic
}

// Convert to string (for display, serialization)
std::string name = Game::Units::troopTypeToString(myTroop);
// Returns "archer"

// Convert from string (for loading, UI input)
Game::Units::TroopType parsed = Game::Units::troopTypeFromString("knight");
// Returns TroopType::Knight
```

## Available Troop Types

- `TroopType::Archer`
- `TroopType::Knight`
- `TroopType::Spearman`
- `TroopType::MountedKnight`

## When to Use Enum vs String

### Use Enum For:
- Internal game logic
- Production systems
- AI decisions
- Nation/troop type definitions
- Factory registration (for troops)
- Configuration lookups

### Use String For:
- UnitComponent (for rendering/audio compatibility)
- UI display
- Audio asset mapping
- Shader names
- External data (maps, configs)
- Building types

## Common Patterns

### Spawning Units

```cpp
Game::Units::SpawnParams sp;
sp.unitType = Game::Units::TroopType::Archer;
sp.position = QVector3D(10.0f, 0.0f, 10.0f);
sp.playerId = 1;

auto unit = factory->create(Game::Units::TroopType::Archer, world, sp);
```

### Production

```cpp
// Set production type
productionComponent->productType = Game::Units::TroopType::Knight;

// Start production from UI (string input)
ProductionService::startProductionForFirstSelectedBarracks(
    world, selectedUnits, playerId, "spearman");
```

### Configuration

```cpp
// Enum-based lookup (preferred)
int individuals = TroopConfig::instance().getIndividualsPerUnit(
    Game::Units::TroopType::Spearman);

// String-based lookup (for compatibility)
int individualsStr = TroopConfig::instance().getIndividualsPerUnit("spearman");
```

### AI Systems

```cpp
AICommand cmd;
cmd.type = AICommandType::StartProduction;
cmd.productType = Game::Units::TroopType::Archer;
```

## Serialization

The serialization system automatically converts between enum and string:

```cpp
// Save (enum -> string)
json["productType"] = troopTypeToString(production->productType);

// Load (string -> enum)
production->productType = troopTypeFromString(json["productType"].toString());
```

## Migration Notes

If you're updating existing code:

1. Replace string literals with enum values:
   ```cpp
   // Old
   if (unitType == "archer") { ... }
   
   // New
   if (unitType == TroopType::Archer) { ... }
   ```

2. Use conversion functions at boundaries:
   ```cpp
   // UI to internal
   TroopType type = troopTypeFromString(qmlString.toStdString());
   
   // Internal to UI
   QString displayName = QString::fromStdString(troopTypeToString(type));
   ```

3. UnitComponent still uses strings - no changes needed there

## Performance Benefits

- Enum comparisons are faster than string comparisons
- No string allocation overhead
- Better CPU cache utilization
- Compiler can optimize switch statements on enums

## Type Safety Benefits

- Typos caught at compile time
- IDE autocomplete for troop types
- Refactoring tools can find all usages
- Impossible to use invalid troop types

## Adding New Troop Types

To add a new troop type:

1. Add enum value to `TroopType` in `troop_type.h`
2. Update `troopTypeToString()` function
3. Update `troopTypeFromString()` function
4. Add configuration in `TroopConfig` constructor
5. Register in factory system
6. Update UI icons/audio mappings as needed

Example:
```cpp
enum class TroopType { 
    Archer, 
    Knight, 
    Spearman, 
    MountedKnight,
    Crossbowman  // New type
};
```
