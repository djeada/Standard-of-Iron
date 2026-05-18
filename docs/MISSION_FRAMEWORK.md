# Mission Framework Documentation

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
      └── second_punic_war.json
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
    "difficulty": "hard",
    "strategy": "aggressive",
    "team_id": 1,
    "commander_troop": "carthage_war_leader",
    "personality": {
      "aggression": 0.7,
      "defense": 0.3,
      "harassment": 0.5
    },
    "starting_buildings": [
      {
        "type": "barracks",
        "position": {"x": 180, "z": 180},
        "max_population": 140
      }
    ],
    "starting_units": [
      {
        "type": "builder",
        "count": 2,
        "position": {"x": 176, "z": 178}
      },
      {
        "type": "spearman",
        "count": 6,
        "position": {"x": 182, "z": 182}
      }
    ],
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

#### AI fields

| Field | Required | Meaning |
| --- | --- | --- |
| `id` | Yes | Mission-local identifier for the AI setup |
| `nation` | Yes | Nation used for roster resolution |
| `faction` | Yes | Faction metadata for mission/UI use |
| `color` | Yes | Player color |
| `difficulty` | No | Execution tuning; omitted falls back to normal behavior |
| `strategy` | No | Base strategic preset; omitted falls back to `balanced` |
| `team_id` | No | Allies AIs with the same team and prevents them fighting each other |
| `commander_troop` | No | Explicit commander troop override |
| `personality` | No | Fine-tunes aggression / defense / harassment on top of the strategy |
| `starting_units` | No | Spawns the AI with units at mission start |
| `starting_buildings` | No | Spawns the AI with structures at mission start |
| `waves` | No | Timed reinforcements layered on top of the regular AI |

#### Supported strategy values

The current AI strategy parser accepts:

- `balanced`
- `aggressive`
- `defensive`
- `expansionist`
- `economic`
- `harasser` or `harassment`
- `rusher` or `rush`

If the string is omitted or unrecognized, the engine falls back to `balanced`.

#### Difficulty values

The current difficulty pipeline is execution-only. It affects things like AI update cadence, production speed, and scouting reach without changing the chosen strategic identity.

- `easy`
- `hard`
- `very_hard`
- `medium` currently behaves like normal/default tuning
- omitted or any other value -> normal/default tuning

#### Personality values

`personality` currently exposes three normalized floats, usually in the `0.0` to `1.0` range:

- `aggression`
- `defense`
- `harassment`

If any value is omitted, it defaults to `0.5`.

These values are applied **after** the base strategy preset, so a `defensive` AI with high aggression still feels defensive overall, but attacks sooner and commits more readily than the default preset.

#### Team ID (Optional)

AI opponents can be assigned to teams using the `team_id` field. AI players with the same `team_id` will be allied and won't attack each other. If `team_id` is not specified, each AI player will be on their own team (enemies to all other AI players).

Example with allied AI opponents:
```json
"ai_setups": [
  {
    "id": "roman_legion_1",
    "nation": "roman_republic",
    "strategy": "balanced",
    "difficulty": "hard",
    "team_id": 1,
    ...
  },
  {
    "id": "roman_legion_2",
    "nation": "roman_republic",
    "strategy": "harasser",
    "difficulty": "hard",
    "team_id": 1,
    ...
  }
]
```

#### Authoring examples

Balanced frontline AI:

```json
{
  "id": "roman_line",
  "nation": "roman_republic",
  "faction": "roman",
  "color": "red",
  "difficulty": "hard",
  "strategy": "balanced",
  "personality": {
    "aggression": 0.6,
    "defense": 0.55,
    "harassment": 0.25
  },
  "starting_buildings": [
    {
      "type": "barracks",
      "position": {"x": 132, "z": 82},
      "max_population": 180
    }
  ],
  "starting_units": [
    {
      "type": "builder",
      "count": 2,
      "position": {"x": 130, "z": 80}
    },
    {
      "type": "spearman",
      "count": 8,
      "position": {"x": 134, "z": 84}
    }
  ]
}
```

Forward pressure harasser:

```json
{
  "id": "numidian_raiders",
  "nation": "carthage",
  "faction": "carthaginian",
  "color": "yellow",
  "difficulty": "hard",
  "strategy": "harasser",
  "personality": {
    "aggression": 0.76,
    "defense": 0.3,
    "harassment": 0.85
  },
  "starting_buildings": [
    {
      "type": "barracks",
      "position": {"x": 12, "z": 78},
      "max_population": 120
    }
  ],
  "starting_units": [
    {
      "type": "builder",
      "count": 1,
      "position": {"x": 14, "z": 80}
    },
    {
      "type": "horse_swordsman",
      "count": 5,
      "position": {"x": 10, "z": 80}
    }
  ]
}
```

### Victory Conditions

Supported victory condition types:

- **destroy_all_enemies**: Eliminate all enemy forces
- **survive_duration**: Survive for specified time (in seconds)
- **control_structures**: Control the specified structure types
- **capture_structures**: Capture the specified structure types from another nation

Victory conditions are evaluated as **OR** conditions: if any configured victory condition
is satisfied, the mission ends in victory.

For the runtime architecture, default-rule behavior, and extension workflow, see
[VICTORY_SYSTEM.md](https://github.com/djeada/Standard-of-Iron/blob/main/docs/VICTORY_SYSTEM.md).

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
- **lose_commander**: Your commander dies
- **only_commander_remaining**: Your commander is the last surviving force after all troops and barracks are gone

Defeat conditions are evaluated as **OR** conditions: if any configured defeat condition
is satisfied, the mission ends in defeat.

If a mission omits `defeat_conditions`, the engine adds the default commander rules:

- `lose_commander`
- `only_commander_remaining`

Example:
```json
"defeat_conditions": [
  {
    "type": "lose_structure",
    "structure_type": "barracks",
    "description": "Do not lose your barracks"
  },
  {
    "type": "lose_commander",
    "description": "Protect Hannibal"
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
  "id": "second_punic_war",
  "title": "Second Punic War",
  "description": "Campaign across the Mediterranean",
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
    ":/assets/campaigns/second_punic_war.json",
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
  // Example: "second_punic_war/defend_outpost"
  
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
- When defeat conditions are omitted, commander-default defeat rules are injected automatically

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
