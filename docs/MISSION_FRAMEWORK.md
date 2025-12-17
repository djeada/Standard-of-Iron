# Mission Framework Documentation

## Overview

The Mission Framework provides a formal authoring layer for creating structured gameplay experiences in Standard of Iron. It separates playable maps from mission logic, allowing designers to create reusable missions and organize them into campaigns.

## Architecture

### Three-Tier Structure

```
assets/
  ├── maps/          # Playable level geometry + terrain
  │   ├── map_forest.json
  │   ├── map_mountain.json
  │   └── map_rivers.json
  ├── missions/      # Gameplay rules that reference maps
  │   ├── defend_outpost.json
  │   └── forest_ambush.json
  └── campaigns/     # Ordered mission collections
      └── tutorial_campaign.json
```

### Components

- **Map**: Pure level geometry, terrain, and environment
- **Mission**: Gameplay setup, objectives, and win/loss conditions
- **Campaign**: Ordered collection of missions with narrative metadata

## Mission Configuration

### File Structure

```json
{
  "id": "defend_outpost",
  "title": "Defend the Outpost",
  "summary": "Hold your position against waves of enemy attacks",
  "map_path": ":/assets/maps/map_forest.json",
  
  "player_setup": { ... },
  "ai_setups": [ ... ],
  "victory_conditions": [ ... ],
  "defeat_conditions": [ ... ],
  "events": [ ... ]
}
```

### Player Setup

Defines the human player's starting configuration:

```json
"player_setup": {
  "nation": "roman_republic",
  "faction": "roman",
  "color": "red",
  "starting_units": [
    {
      "type": "spearman",
      "count": 10,
      "position": {"x": 60, "z": 60}
    }
  ],
  "starting_buildings": [
    {
      "type": "barracks",
      "position": {"x": 60, "z": 60},
      "max_population": 200
    }
  ],
  "starting_resources": {
    "gold": 1000,
    "food": 500
  }
}
```

### AI Setup

Defines AI opponents with personality and behavior:

```json
"ai_setups": [
  {
    "id": "enemy_1",
    "nation": "carthage",
    "faction": "carthaginian",
    "color": "blue",
    "difficulty": "medium",
    "personality": {
      "aggression": 0.7,
      "defense": 0.3,
      "harassment": 0.5
    },
    "waves": [
      {
        "timing": 120.0,
        "composition": [
          {"type": "swordsman", "count": 8},
          {"type": "archer", "count": 4}
        ],
        "entry_point": {"x": 190, "z": 190}
      }
    ]
  }
]
```

### Victory Conditions

Supported victory condition types:

- **destroy_all_enemies**: Eliminate all enemy forces
- **survive_duration**: Survive for specified time (in seconds)
- **hold_capture_points**: Control specific map locations
- **escort_unit**: Safely move a unit to a destination
- **reach_resource_quota**: Accumulate specified resources

Example:
```json
"victory_conditions": [
  {
    "type": "survive_duration",
    "duration": 600.0,
    "description": "Survive for 10 minutes"
  }
]
```

### Defeat Conditions

Supported defeat condition types:

- **lose_structure**: Specific building type is destroyed
- **lose_all_units**: All units are eliminated
- **timer_expires**: Time limit exceeded
- **lose_capture_points**: Control of key locations lost

Example:
```json
"defeat_conditions": [
  {
    "type": "lose_structure",
    "structure_type": "barracks",
    "description": "Do not lose your barracks"
  }
]
```

### Events

Timed or state-based triggers for dynamic gameplay:

```json
"events": [
  {
    "trigger": {
      "type": "timer",
      "time": 300.0
    },
    "actions": [
      {
        "type": "show_message",
        "text": "Reinforcements approaching!"
      }
    ]
  }
]
```

## Campaign Configuration

### File Structure

```json
{
  "id": "tutorial_campaign",
  "title": "Roman Conquest Tutorial",
  "description": "Learn the basics of warfare",
  "missions": [
    {
      "mission_id": "forest_ambush",
      "order_index": 0,
      "intro_text": "Your first task...",
      "outro_text": "Well done!",
      "difficulty_modifier": 1.0
    }
  ]
}
```

### Mission Ordering

- `order_index` must be contiguous starting from 0 or 1
- Missions are presented in order
- Validator ensures proper sequencing

## Content Validation

### Validator CLI

Run content validation:

```bash
# Via CMake
cmake --build build --target validate-content

# Via Makefile
make validate-content

# Direct execution
./build/bin/content_validator assets
```

### Validation Checks

The validator performs:

1. **JSON Schema Validation**
   - Proper structure and required fields
   - Correct data types

2. **Asset Reference Validation**
   - Referenced maps exist
   - Campaign missions reference valid mission files

3. **Campaign Validation**
   - Mission order indices are contiguous
   - Starts at 0 or 1

4. **Cross-Reference Checks**
   - Units, buildings, and entity types are valid
   - No circular dependencies

### Build Integration

Add to your build process:

```cmake
# Make validation part of the build
add_dependencies(standard_of_iron validate-content)
```

When uncommented, invalid content will fail the build.

## Usage in Code

### Loading Missions

```cpp
#include "game/map/mission_loader.h"

Game::Mission::MissionDefinition mission;
QString error;

if (!Game::Mission::MissionLoader::loadFromJsonFile(
    ":/assets/missions/defend_outpost.json", 
    mission, 
    &error)) {
  qWarning() << "Failed to load mission:" << error;
  return;
}

// Use mission data
qInfo() << "Loaded mission:" << mission.title;
```

### Loading Campaigns

```cpp
#include "game/map/campaign_loader.h"

Game::Campaign::CampaignDefinition campaign;
QString error;

if (!Game::Campaign::CampaignLoader::loadFromJsonFile(
    ":/assets/campaigns/tutorial_campaign.json",
    campaign,
    &error)) {
  qWarning() << "Failed to load campaign:" << error;
  return;
}

// Iterate missions
for (const auto& mission : campaign.missions) {
  qInfo() << "Mission" << mission.order_index << ":" << mission.mission_id;
}
```

### Starting a Mission

```cpp
// In GameEngine
void GameEngine::start_campaign_mission(const QString &mission_path) {
  // mission_path format: "campaign_id/mission_id"
  // Example: "tutorial_campaign/defend_outpost"
  
  // Mission loader parses JSON and configures:
  // - Player and AI setups
  // - Victory/defeat conditions
  // - Map reference
}
```

## UI Integration

### Campaign Selection

The CampaignMenu displays available campaigns and their missions:

1. User selects a campaign
2. Mission list appears with:
   - Order number
   - Mission name
   - Intro text preview
3. User selects specific mission
4. Mission loads with proper configuration

### Victory Conditions

Mission-specific victory conditions override map defaults:

- VictoryService configured from mission JSON
- Win/loss checks match mission requirements
- Campaign progress tracked on completion

## Best Practices

### Mission Design

1. **Keep Maps Reusable**: Don't hardcode gameplay in map files
2. **Clear Objectives**: Provide explicit victory/defeat conditions
3. **Balanced Difficulty**: Test with various player skill levels
4. **Progressive Complexity**: Early missions should be simpler

### Campaign Structure

1. **Logical Progression**: Order missions by difficulty
2. **Narrative Flow**: Use intro/outro text for story
3. **Varied Gameplay**: Mix victory condition types
4. **Testing**: Validate entire campaign sequences

### Asset Organization

```
assets/missions/
  ├── 01_tutorial/
  │   ├── basic_combat.json
  │   └── defend_base.json
  ├── 02_main_campaign/
  │   ├── forest_battle.json
  │   └── siege_warfare.json
  └── 03_advanced/
      └── survival_challenge.json
```

## Troubleshooting

### Common Validation Errors

**"Mission not found"**
- Check mission_id matches filename
- Ensure mission file is in assets/missions/

**"Order index not contiguous"**
- Campaign missions must have sequential indices
- No gaps allowed (0, 1, 2, ... or 1, 2, 3, ...)

**"Referenced map not found"**
- Verify map_path is correct
- Check map file exists in assets/maps/

### Debugging

Enable verbose logging:
```bash
QT_LOGGING_RULES="*.debug=true" ./standard_of_iron
```

Check mission loading:
```cpp
qDebug() << "Loading mission:" << mission.id;
qDebug() << "Victory conditions:" << mission.victory_conditions.size();
```

## Future Enhancements

Planned features:

- [ ] Dynamic mission generation
- [ ] User-created campaigns
- [ ] Mission editor UI
- [ ] Achievement tracking
- [ ] Difficulty scaling system
- [ ] Replay system integration

## API Reference

See also:
- `game/map/mission_definition.h` - Mission data structures
- `game/map/campaign_definition.h` - Campaign data structures
- `game/map/mission_loader.h` - Mission loading API
- `game/map/campaign_loader.h` - Campaign loading API
- `tools/content_validator/` - Validation implementation
