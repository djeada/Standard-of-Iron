# Assets Structure

This directory contains all game assets organized by type.

## Directory Structure

```
assets/
├── audio/              # Sound effects and music
├── campaigns/          # Campaign definitions
│   └── *.json         # Campaign files linking missions
├── data/              # Game data and configurations
│   ├── nations/       # Nation-specific data
│   └── troops/        # Troop definitions
├── maps/              # Playable level files
│   └── *.json        # Map geometry and terrain
├── meshes/            # 3D models
├── missions/          # Mission definitions
│   └── *.json        # Mission objectives and setup
├── shaders/           # GLSL shaders
├── textures/          # Image files
└── visuals/           # Visual configurations
    ├── emblems/       # Nation emblems
    └── icons/         # Unit icons
```

## Mission Framework

### Maps
Located in `maps/`, these files contain:
- Level geometry and terrain
- Biome settings
- Camera configuration
- Spawn points (overridden by missions)
- Environmental effects

Maps are **reusable** and should not contain mission-specific logic.

### Missions
Located in `missions/`, these files define:
- Player and AI setup
- Victory/defeat conditions
- Starting units and buildings
- AI behavior and waves
- Timed events

Missions reference exactly one map and can customize the gameplay experience.

### Campaigns
Located in `campaigns/`, these files contain:
- Campaign metadata (id, title, description)
- Ordered list of missions
- Mission-specific intro/outro text
- Optional difficulty modifiers

Campaigns provide narrative structure and progression.

## File Formats

### Mission Files (`.json`)
```json
{
  "id": "unique_mission_id",
  "title": "Display Name",
  "summary": "Brief description",
  "map_path": ":/assets/maps/map_name.json",
  "player_setup": { ... },
  "ai_setups": [ ... ],
  "victory_conditions": [ ... ],
  "defeat_conditions": [ ... ],
  "events": [ ... ]
}
```

### Campaign Files (`.json`)
```json
{
  "id": "unique_campaign_id",
  "title": "Campaign Name",
  "description": "Campaign description",
  "missions": [
    {
      "mission_id": "mission_file_name",
      "order_index": 0,
      "intro_text": "Optional mission intro",
      "outro_text": "Optional mission outro"
    }
  ]
}
```

### Map Files (`.json`)
Standard map format with:
- Grid dimensions
- Biome configuration
- Terrain features
- Rivers, roads, bridges
- Spawn points (template, overridden by missions)

## Content Validation

Validate all mission and campaign files:

```bash
# Using Makefile
make validate-content

# Using CMake
cmake --build build --target validate-content

# Direct execution
./build/bin/content_validator assets
```

The validator checks:
- JSON syntax and structure
- Required fields present
- Asset references valid
- Campaign mission ordering
- No circular dependencies

## Adding New Content

### Creating a New Mission

1. Create `assets/missions/my_mission.json`
2. Define mission parameters
3. Reference an existing map
4. Run validator: `make validate-content`

### Creating a New Campaign

1. Create `assets/campaigns/my_campaign.json`
2. List missions in order
3. Set contiguous order_index values
4. Run validator: `make validate-content`

### Creating a New Map

1. Create `assets/maps/my_map.json`
2. Define terrain and biome
3. Set grid dimensions
4. Add terrain features
5. Mission files can reference this map

## Resource Paths

Assets are embedded in the application using Qt Resource System:

- Resource prefix: `:/assets/`
- Example: `:/assets/maps/map_forest.json`
- Example: `:/assets/missions/defend_outpost.json`

When adding new assets, update `assets.qrc` to include them in the build.

## Documentation

See [MISSION_FRAMEWORK.md](../docs/MISSION_FRAMEWORK.md) for detailed documentation on:
- Mission configuration options
- Victory/defeat condition types
- AI setup and personality
- Event system
- Campaign structure
- Code integration

## Examples

Example mission and campaign files are provided:

- `missions/defend_outpost.json` - Survival mission
- `missions/forest_ambush.json` - Elimination mission
- `campaigns/second_punic_war.json` - Multi-mission campaign

Use these as templates for creating new content.
