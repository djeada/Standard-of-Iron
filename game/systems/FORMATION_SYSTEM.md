# Formation System Documentation

## Overview

The Formation System provides nation-specific troop gathering and positioning logic for AI-controlled armies. Instead of having all units cluster at a single rally point, troops organize themselves according to their nation's preferred formation style.

## Architecture

### Core Components

#### 1. `IFormation` Interface
The base interface that all formation types must implement:
```cpp
class IFormation {
public:
  virtual std::vector<QVector3D>
  calculatePositions(int unitCount, const QVector3D &center,
                     float baseSpacing = 1.0f) const = 0;

  virtual FormationType getType() const = 0;
};
```

#### 2. Formation Types
Currently implemented formation types:
- **Roman**: Tight, defensive rectangular formation with closely-spaced units
- **Barbarian**: Loose, scattered formation with randomized positioning

#### 3. `FormationSystem` (Singleton)
The central system that manages formation types and calculates positions:
```cpp
FormationSystem::instance().getFormationPositions(
    formationType, unitCount, center, baseSpacing);
```

### Integration with Nation System

Each `Nation` struct now includes a `formationType` field:
```cpp
struct Nation {
  std::string id;
  std::string displayName;
  FormationType formationType = FormationType::Roman;
  // ... other fields
};
```

### AI Integration

The formation system is integrated into AI behaviors:

1. **GatherBehavior**: Uses nation-specific formations when gathering troops at rally points
2. **DefendBehavior**: Uses nation-specific formations when positioning defensive units

Both behaviors query the nation for the player:
```cpp
const Nation *nation = 
    NationRegistry::instance().getNationForPlayer(context.playerId);
FormationType formationType = nation->formationType;
```

## Formation Implementations

### Roman Formation
- **Characteristics**: Tight, disciplined rectangular formation
- **Spacing**: 1.2x base spacing
- **Layout**: Rectangular grid with slightly compressed depth (0.9x)
- **Use Case**: Defensive positioning, shield wall tactics

**Implementation Details**:
- Calculates rows based on unit count with 0.7 ratio
- Evenly distributes units across columns
- Centers the formation around the rally point

### Barbarian Formation
- **Characteristics**: Loose, scattered positioning
- **Spacing**: 1.8x base spacing  
- **Layout**: Grid with random jitter for irregular appearance
- **Use Case**: Aggressive, mobile warfare

**Implementation Details**:
- Uses square grid as base
- Applies random offset (±30% of spacing) to each unit
- Fixed seed (42) for deterministic but scattered placement

## Adding New Formations

To add a new formation type:

### 1. Define the Formation Type
Add to `FormationType` enum in `formation_system.h`:
```cpp
enum class FormationType { 
  Roman, 
  Barbarian, 
  Greek,      // New formation
  Egyptian    // New formation
};
```

### 2. Implement the Formation Class
Create a new class implementing `IFormation`:
```cpp
class GreekFormation : public IFormation {
public:
  std::vector<QVector3D> calculatePositions(
      int unitCount, const QVector3D &center,
      float baseSpacing = 1.0f) const override {
    
    // Implement Greek phalanx logic here
    // Example: Very tight front line, multiple rows
    
    std::vector<QVector3D> positions;
    // ... formation logic ...
    return positions;
  }

  FormationType getType() const override { 
    return FormationType::Greek; 
  }
};
```

### 3. Register the Formation
Add registration in `FormationSystem::initializeDefaults()`:
```cpp
void FormationSystem::initializeDefaults() {
  registerFormation(FormationType::Roman,
                    std::make_unique<RomanFormation>());
  registerFormation(FormationType::Barbarian,
                    std::make_unique<BarbarianFormation>());
  registerFormation(FormationType::Greek,
                    std::make_unique<GreekFormation>());  // New
}
```

### 4. Assign to Nations
In `NationRegistry::initializeDefaults()`, assign the formation to nations:
```cpp
Nation greekCityStates;
greekCityStates.id = "greek_city_states";
greekCityStates.displayName = "Greek City States";
greekCityStates.formationType = FormationType::Greek;
// ... configure troops ...
registerNation(std::move(greekCityStates));
```

## Design Patterns

### Strategy Pattern
The formation system uses the Strategy pattern:
- `IFormation` is the strategy interface
- `RomanFormation`, `BarbarianFormation` are concrete strategies
- `FormationSystem` is the context that uses strategies

### Singleton Pattern
`FormationSystem` uses the singleton pattern to provide global access while ensuring single instance.

### Factory-like Registration
The system uses a registration mechanism allowing runtime addition of formations without modifying existing code.

## File Structure

```
game/systems/
├── formation_system.h          # Formation interfaces and system
├── formation_system.cpp        # Formation implementations
├── nation_registry.h           # Nation struct with formationType
├── nation_registry.cpp         # Nation initialization with formations
└── ai_system/
    └── behaviors/
        ├── gather_behavior.cpp  # Uses formations for gathering
        └── defend_behavior.cpp  # Uses formations for defense
```

## Future Enhancements

Potential areas for expansion:

1. **Dynamic Formations**: Formations that change based on unit types (melee front, ranged back)
2. **Terrain-Aware Formations**: Adjust formation based on terrain (hills, valleys)
3. **Formation Bonuses**: Apply combat bonuses when units maintain formation
4. **Formation Transitions**: Smooth transitions between different formations
5. **Custom Formation Editor**: Allow players to design custom formations
6. **Unit-Specific Spacing**: Different spacing for cavalry, infantry, archers
7. **Formation States**: Attack, defend, march formations

## Testing

The formation system has been verified to:
- ✓ Provide separate module for formation logic
- ✓ Implement Roman formation (tight rectangular)
- ✓ Implement Barbarian formation (loose scattered)
- ✓ Integrate with Nation system
- ✓ Work with AI GatherBehavior
- ✓ Work with AI DefendBehavior
- ✓ Support easy addition of new formations

## Example Usage

```cpp
// Get formation positions for 20 units
const Nation* nation = NationRegistry::instance().getNationForPlayer(playerId);
QVector3D rallyPoint(100.0f, 0.0f, 100.0f);

auto positions = FormationSystem::instance().getFormationPositions(
    nation->formationType,  // Roman or Barbarian
    20,                     // Unit count
    rallyPoint,            // Center position
    1.5f                   // Base spacing
);

// positions now contains 20 QVector3D positions arranged in formation
```

## Benefits

1. **Modularity**: Formation logic is separated from AI behaviors
2. **Extensibility**: Easy to add new formations without modifying existing code
3. **Nation Identity**: Each nation has unique tactical appearance
4. **Reusability**: Same formation system used by multiple AI behaviors
5. **Maintainability**: Clear interfaces and single responsibility
6. **Testability**: Formation logic can be tested independently

## Conclusion

The Formation System successfully replaces the simple clustering behavior with nation-specific formations, providing:
- More realistic troop positioning
- Nation-specific tactical identity
- Flexible architecture for future expansion
- Clean integration with existing AI systems
