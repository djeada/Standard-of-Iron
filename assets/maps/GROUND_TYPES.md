# Ground Type System

The ground type system allows designers to quickly select from predefined ground variations when creating maps. Each ground type provides a different visual appearance for the terrain.

## Available Ground Types

| Ground Type ID  | Description                        | Visual Characteristics                                 |
|-----------------|------------------------------------|---------------------------------------------------------|
| `forest_mud`    | Deep Green + Mud Ground (Default)  | Lush green grass with dark brown muddy soil            |
| `grass_dry`     | Dry Mediterranean Grass            | Yellowish-green grass with light brown dusty soil      |
| `soil_rocky`    | Light-Brown Rocky Soil             | Sparse grass with prominent light brown rocks          |
| `alpine_mix`    | Alpine Rock + Snow Mix             | Cool-toned grass with light gray/white rocky terrain   |
| `soil_fertile`  | Dark Fertile Farmland Soil         | Rich green grass with very dark brown fertile soil     |

## Usage

Add the `groundType` property to the `biome` section of your map JSON file:

```json
{
  "name": "My Alpine Map",
  "biome": {
    "groundType": "alpine_mix",
    "seed": 12345
  }
}
```

## Default Behavior

If no `groundType` is specified, the system defaults to `forest_mud`, which is the original ground style.

## Overriding Ground Type Defaults

Ground type presets can be overridden with explicit values. The ground type defaults are applied first, then any explicit values in the JSON are applied on top:

```json
{
  "biome": {
    "groundType": "alpine_mix",
    "grassPrimary": [0.2, 0.5, 0.2],
    "seed": 54321
  }
}
```

In this example, the map will use all alpine_mix defaults except for `grassPrimary`, which is explicitly set.

## Backward Compatibility

Existing maps without the `groundType` property will continue to work exactly as before, using the default `forest_mud` ground type settings.

## Example: Complete Map with Ground Type

```json
{
  "name": "Mediterranean Battlefield",
  "coordSystem": "grid",
  "grid": {
    "width": 100,
    "height": 100,
    "tileSize": 1.0
  },
  "biome": {
    "groundType": "grass_dry",
    "seed": 98765,
    "patchDensity": 3.0,
    "plantDensity": 0.4
  },
  "camera": {
    "center": [50, 0, 50],
    "distance": 30.0,
    "tiltDeg": 45.0,
    "yaw": 225.0
  }
}
```

## Ground Type Color Presets

### forest_mud (Default)
- **Grass Primary:** Deep green (0.30, 0.60, 0.28)
- **Grass Secondary:** Medium green (0.44, 0.70, 0.32)
- **Grass Dry:** Olive (0.72, 0.66, 0.48)
- **Soil Color:** Dark brown (0.28, 0.24, 0.18)
- **Rock Low:** Gray (0.48, 0.46, 0.44)
- **Rock High:** Light gray (0.68, 0.69, 0.73)

### grass_dry
- **Grass Primary:** Dry yellow-green (0.55, 0.52, 0.30)
- **Grass Secondary:** Tan-green (0.62, 0.58, 0.35)
- **Grass Dry:** Golden (0.75, 0.68, 0.42)
- **Soil Color:** Sandy brown (0.45, 0.38, 0.28)
- **Rock Low:** Warm gray (0.58, 0.55, 0.50)
- **Rock High:** Light warm gray (0.72, 0.70, 0.65)

### soil_rocky
- **Grass Primary:** Muted green (0.42, 0.48, 0.30)
- **Grass Secondary:** Sage (0.50, 0.54, 0.35)
- **Grass Dry:** Tan (0.60, 0.55, 0.40)
- **Soil Color:** Medium brown (0.50, 0.42, 0.32)
- **Rock Low:** Brownish gray (0.55, 0.52, 0.48)
- **Rock High:** Warm light gray (0.70, 0.68, 0.65)

### alpine_mix
- **Grass Primary:** Cool green (0.35, 0.42, 0.32)
- **Grass Secondary:** Teal-green (0.40, 0.48, 0.38)
- **Grass Dry:** Cool gray-green (0.55, 0.52, 0.45)
- **Soil Color:** Cool gray (0.38, 0.35, 0.32)
- **Rock Low:** Bluish gray (0.60, 0.62, 0.65)
- **Rock High:** Near white/snow (0.85, 0.88, 0.92)

### soil_fertile
- **Grass Primary:** Rich green (0.28, 0.52, 0.25)
- **Grass Secondary:** Vibrant green (0.38, 0.62, 0.32)
- **Grass Dry:** Olive-brown (0.55, 0.50, 0.35)
- **Soil Color:** Very dark brown (0.22, 0.18, 0.14)
- **Rock Low:** Dark gray (0.42, 0.40, 0.38)
- **Rock High:** Medium gray (0.55, 0.54, 0.52)
